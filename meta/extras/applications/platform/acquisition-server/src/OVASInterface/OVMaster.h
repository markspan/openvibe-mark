#include <QtCore>
#include <QtQml/qqmlregistration.h>


namespace OpenViBE {
namespace AcquisitionServer {

class OVMaster : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(QStringList drivers READ getDrivers NOTIFY driverChanged)
public:
    OVMaster();
    ~OVMaster();

    QStringList getDrivers();

signals:
    void driverChanged();

protected:
    struct OVMasterPrivate *d = nullptr;
};

}  // namespace AcquisitionServer
}  // namespace OpenViBE