#include "OVMaster.h"

#include <system/ovCTime.h>

#include "ovasIDriver.h"
#include "ovasCAcquisitionServer.h"

// Simulation drivers
#include "generic-oscillator/ovasCDriverGenericOscillator.h"

/*
#include "generic-sawtooth/ovasCDriverGenericSawTooth.h"
#include "generic-time-signal/ovasCDriverGenericTimeSignal.h"
#include "simulated-deviator/ovasCDriverSimulatedDeviator.h"

#include "biosemi-activetwo/ovasCDriverBioSemiActiveTwo.h"
#include "brainproducts-actichamp/ovasCDriverBrainProductsActiCHamp.h"
#include "brainproducts-brainampseries/ovasCDriverBrainProductsBrainampSeries.h"
#include "brainproducts-vamp/ovasCDriverBrainProductsVAmp.h"
#include "brainproducts-liveamp/ovasCDriverBrainProductsLiveAmp.h"
#include "egi-ampserver/ovasCDriverEGIAmpServer.h"
#include "emotiv-epoc/ovasCDriverEmotivEPOC.h"
#include "labstreaminglayer/ovasCDriverLabStreamingLayer.h"
#include "micromed-systemplusevolution/ovasCDriverMicromedSystemPlusEvolution.h"
#include "mindmedia-nexus32b/ovasCDriverMindMediaNeXus32B.h"
#include "mcs-nvx/ovasCDriverMCSNVXDriver.h"
#include "neuroelectrics-enobio3g/ovasCDriverEnobio3G.h"
#include "neuroservo/ovasCDriverNeuroServoHid.h"
#include "neurosky-mindset/ovasCDriverNeuroskyMindset.h"
#include "tmsi/ovasCDriverTMSi.h"
#include "tmsi-refa32b/ovasCDriverTMSiRefa32B.h"

#include "mensia-acquisition/ovasCDriverMensiaAcquisition.h"

#include "shimmer-gsr/ovasCDriverShimmerGSR.hpp"
*/

#include <vector>

namespace OpenViBE {
namespace AcquisitionServer {

struct OVMasterPrivate {

    OVMasterPrivate(const Kernel::IKernelContext& kernelCtx):
        m_acquisitionServer(new CAcquisitionServer(kernelCtx)) {}

    void initDrivers();
    void launchThread();

    std::vector<IDriver*> m_drivers;
    CAcquisitionServer* m_acquisitionServer = nullptr;
};

void OVMasterPrivate::initDrivers()
{

    m_drivers.push_back(new CDriverGenericOscillator(m_acquisitionServer->getDriverContext()));
    //m_drivers.push_back(new CDriverGenericSawTooth(m_acquisitionServer->getDriverContext()));
    //m_drivers.push_back(new CDriverGenericTimeSignal(m_acquisitionServer->getDriverContext()));
}

void OVMasterPrivate::launchThread()
{

}

OVMaster::OVMaster() {
    OpenViBE::CKernelLoader kernelLoader;

	std::cout << "[  INF  ] Created kernel loader, trying to load kernel module" << std::endl;
	OpenViBE::CString error;

	OpenViBE::CString kernelFile = OpenViBE::Directories::getLib("kernel");

	if (!kernelLoader.load(kernelFile, &error)) { 
        std::cout << "[ FAILED ] Error loading kernel from [" << kernelFile << "]: " << error << "\n"; 
    }
	else {
		std::cout << "[  INF  ] Kernel module loaded, trying to get kernel descriptor" << std::endl;
		OpenViBE::Kernel::IKernelDesc* kernelDesc = nullptr;
		kernelLoader.initialize();
		kernelLoader.getKernelDesc(kernelDesc);
		if (!kernelDesc) { std::cout << "[ FAILED ] No kernel descriptor" << std::endl; }
		else {
			std::cout << "[  INF  ] Got kernel descriptor, trying to create kernel" << std::endl;


			OpenViBE::CString configFile = OpenViBE::CString(OpenViBE::Directories::getDataDir() + "/kernel/openvibe.conf");

			OpenViBE::Kernel::IKernelContext* kernelCtx = kernelDesc->createKernel("acquisition-server", configFile);
			if (!kernelCtx) { std::cout << "[ FAILED ] No kernel created by kernel descriptor" << std::endl; }
			else {
				kernelCtx->initialize();

				OpenViBE::Kernel::IConfigurationManager& configManager = kernelCtx->getConfigurationManager();

				// @FIXME CERT silent fail if missing file is provided
				configManager.addConfigurationFromFile(configManager.expand("${Path_Data}/applications/acquisition-server/acquisition-server-defaults.conf"));

				// User configuration mods
				configManager.addConfigurationFromFile(configManager.expand("${Path_UserData}/openvibe-acquisition-server.conf"));

				kernelCtx->getPluginManager().addPluginsFromFiles(configManager.expand("${AcquisitionServer_Plugins}"));

                /*
                Commented out cause config is loaded from argc/argv in legacy, which is not in place here
				for (auto itr = config.tokens.begin(); itr != config.tokens.end(); ++itr) {
					kernelCtx->getLogManager() << OpenViBE::Kernel::LogLevel_Trace << "Adding command line configuration token ["
							<< (*itr).first.c_str() << " = " << (*itr).second.c_str() << "]\n";
					configManager.addOrReplaceConfigurationToken((*itr).first.c_str(), (*itr).second.c_str());
				}   */

				// Check the clock
				if (!System::Time::isClockSteady()) {
					kernelCtx->getLogManager() << OpenViBE::Kernel::LogLevel_Warning
							<< "The system does not seem to have a steady clock. This may affect the acquisition time precision.\n";
				}


                d = new OVMasterPrivate(*kernelCtx);
                d->initDrivers();
                d->launchThread();
                emit driverChanged();
            }
        }
    }


}

QStringList OVMaster::getDrivers() {
    QStringList drivers;
    for (const auto driver: d->m_drivers) {
        std::cout << Q_FUNC_INFO << " - Adding driver: " << driver->getName() << std::endl;
        drivers.append(driver->getName());
    }
    return drivers;
}

OVMaster::~OVMaster() {
    delete d;

}


}  // namespace AcquisitionServer
}  // namespace OpenViBE