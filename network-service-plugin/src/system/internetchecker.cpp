#include "internetchecker.h"

#include "httpmanager.h"
#include "settingconfig.h"
#include "constants.h"

#include <QDebug>

#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/ActiveConnection>
#include <NetworkManagerQt/Ipv4Setting>
#include <NetworkManagerQt/Ipv6Setting>

using namespace network::systemservice;

InternetChecker::InternetChecker(QObject *parent)
    : QObject(parent)
{
}

InternetChecker::~InternetChecker()
{
}

void InternetChecker::switchInternetAccess()
{
    // 没有主连接无需处理，之前怎么走就怎么走
    NetworkManager::ActiveConnection::Ptr primaruyConnection = NetworkManager::primaryConnection();
    if (primaruyConnection.isNull())
        return;

    NetworkManager::Device::List avaibleDevice;
    NetworkManager::Device::List devices = NetworkManager::networkInterfaces();
    for (const NetworkManager::Device::Ptr &device: devices) {
        // 这里只检查已经激活的连接
        if (device->state() != NetworkManager::Device::State::Activated)
            continue;

        avaibleDevice << device;
    }

    // 只处理大于2个网卡的情况
    if (avaibleDevice.size() <= 1)
        return;

    // 遍历每个网卡，检测哪些网卡可以上网
    NetworkManager::Device::List accessibleDevices;
    for (const NetworkManager::Device::Ptr &device : avaibleDevice) {
        bool canAccess = canDeviceAccessInternet(device);
        qCDebug(DSM) << "Check device" << device->interfaceName()
                     << "type:" << device->type()
                     << "can access internet:" << canAccess;
        if (canAccess) {
            accessibleDevices << device;
        }
    }

    if (accessibleDevices.isEmpty())
        return;

    // 找到当前主连接使用的网卡
    QStringList primaryDeviceUnis = primaruyConnection->devices();
    NetworkManager::Device::Ptr primaryDevice;
    for (const NetworkManager::Device::Ptr &device : avaibleDevice) {
        if (primaryDeviceUnis.contains(device->uni())) {
            primaryDevice = device;
            break;
        }
    }

    // 如果当前主连接的网卡可以上网，无需处理
    if (primaryDevice && accessibleDevices.contains(primaryDevice)) {
        qCDebug(DSM) << "Primary device" << primaryDevice->interfaceName() << "can access internet, no switch needed";
        return;
    }

    // 在所有可以上网的网卡中选择最优的网卡
    NetworkManager::Device::Ptr targetDevice;
    // TODO: 实现网卡选择策略
    // 输入: accessibleDevices - 所有可以上网的网卡列表
    // 要求:
    //   1. 如果同时存在有线网卡(Ethernet)和无线网卡(Wifi)，优先选择第一个有线网卡
    //   2. 其他情况，选择列表中第一个可以上网的网卡
    // 可用API:
    //   device->type() == NetworkManager::Device::Ethernet  // 判断是否是有线网卡
    //   device->type() == NetworkManager::Device::Wifi       // 判断是否是无线网卡

    if (targetDevice.isNull())
        return;

    // 通过updateUnsaved将当前主连接的never-default设为true，让NM自动将默认路由切换到目标网卡
    // updateUnsaved只修改内存中的配置，不会持久化到磁盘，连接断开或NM重启后自动恢复
    NetworkManager::Connection::Ptr primaryConn = primaryDevice->activeConnection()->connection();
    NetworkManager::Setting::Ptr primaryIpv4Setting = primaryConn->settings()->setting(NetworkManager::Setting::Ipv4);
    if (primaryIpv4Setting) {
        NetworkManager::Ipv4Setting::Ptr ipv4 = primaryIpv4Setting.dynamicCast<NetworkManager::Ipv4Setting>();
        ipv4->setNeverDefault(true);
        primaryConn->updateUnsaved({{ipv4->name(), ipv4->toMap()}});
    }
    NetworkManager::Setting::Ptr primaryIpv6Setting = primaryConn->settings()->setting(NetworkManager::Setting::Ipv6);
    if (primaryIpv6Setting) {
        NetworkManager::Ipv6Setting::Ptr ipv6 = primaryIpv6Setting.dynamicCast<NetworkManager::Ipv6Setting>();
        ipv6->setNeverDefault(true);
        primaryConn->updateUnsaved({{ipv6->name(), ipv6->toMap()}});
    }
    qCInfo(DSM) << "Set current primary device" << primaryDevice->interfaceName() << "never-default to true (unsaved), switch to" << targetDevice->interfaceName();
}

bool InternetChecker::canDeviceAccessInternet(const NetworkManager::Device::Ptr &device) const
{
    const QStringList checkUrls = SettingConfig::instance()->networkCheckerUrls();
    for (const QString &url : checkUrls) {
        network::service::HttpManager http;
        network::service::HttpReply *httpReply = http.get(url, device->interfaceName());
        if (httpReply->httpCode() != 0) {
            return true;
        }
    }
    return false;
}
