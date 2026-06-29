#pragma once

#if defined(TARGET_HAS_ThirdPartyEEGOAPI)

#include <ovasIDriver.h>
#include <../ovasCConfigurationBuilder.h>
#include <gtk/gtk.h>
#include <vector>

#include "ovasCHeaderEEGO_UG.h"

namespace OpenViBE {
namespace AcquisitionServer {

class CConfigurationEEGO_UG final : public CConfigurationBuilder
{
public:
	CConfigurationEEGO_UG(IDriverContext& ctx, const char* gtkBuilderFilename, CHeaderEEGO_UG& eegoHeader,
	                       uint32_t nEEGChannels = 64, uint32_t nBIPChannels = 24,
	                       std::vector<int>    samplingRates = {},
	                       std::vector<double> eegRanges    = {},
	                       std::vector<double> bipRanges    = {});

	bool preConfigure() override;
	bool postConfigure() override;

protected:
	IDriverContext& m_driverCtx;

private:
	CHeaderEEGO_UG& m_eegoHeader;

	uint32_t m_nEEGChannels = 64;
	uint32_t m_nBIPChannels = 24;

	std::vector<int>    m_samplingRates;
	std::vector<double> m_eegRanges;
	std::vector<double> m_bipRanges;

	GtkComboBox* m_eegRangeComboBox = nullptr;
	GtkComboBox* m_bipRangeComboBox = nullptr;
};

}  // namespace AcquisitionServer
}  // namespace OpenViBE

#endif
