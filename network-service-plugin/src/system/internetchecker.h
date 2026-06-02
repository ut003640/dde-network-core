#ifndef INTERNETCHECKER_H
#define INTERNETCHECKER_H

#include <QObject>
#include <NetworkManagerQt/Device>

namespace network {
namespace systemservice {

class InternetChecker : public QObject
{
    Q_OBJECT

public:
    explicit InternetChecker(QObject *parent = nullptr);
    ~InternetChecker() override;
    void switchInternetAccess();

private:
    bool canDeviceAccessInternet(const NetworkManager::Device::Ptr &device) const;
};

}
}

#endif // INTERNETCHECKER_H
