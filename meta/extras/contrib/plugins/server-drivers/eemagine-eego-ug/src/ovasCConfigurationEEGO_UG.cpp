#if defined(TARGET_HAS_ThirdPartyEEGOAPI)

#include <cmath>
#include <sstream>

#include "ovasCConfigurationEEGO_UG.h"

namespace OpenViBE {
namespace AcquisitionServer {

// Function to set a predefined string in the combobox.
// Copied from ovasCConfigurationBuilder. Seems to be OK, albeit it is strange to have the code duplication,
// but other amplifier drivers do it too.
// Would be nicer if the method would move to some utilities provider.
static void GTKComboBoxSetActiveText(GtkComboBox* box, const gchar* text)
{
	GtkTreeModel* treeModel = gtk_combo_box_get_model(box);
	GtkTreeIter it;
	int index   = 0;
	gchar* name = nullptr;
	if (gtk_tree_model_get_iter_first(treeModel, &it))
	{
		do
		{
			gtk_tree_model_get(treeModel, &it, 0, &name, -1);
			if (std::string(name) == std::string(text))
			{
				gtk_combo_box_set_active(box, index);
				return;
			}
			index++;
		} while (gtk_tree_model_iter_next(treeModel, &it));
	}
}

// If you added more reference attribute, initialize them here
CConfigurationEEGO_UG::CConfigurationEEGO_UG(IDriverContext& ctx, const char* gtkBuilderFilename, CHeaderEEGO_UG& eegoHeader,
                                       uint32_t nEEGChannels, uint32_t nBIPChannels,
                                       std::vector<int> samplingRates,
                                       std::vector<double> eegRanges,
                                       std::vector<double> bipRanges)
	: CConfigurationBuilder(gtkBuilderFilename), m_driverCtx(ctx), m_eegoHeader(eegoHeader)
	  , m_nEEGChannels(nEEGChannels), m_nBIPChannels(nBIPChannels)
	  , m_samplingRates(std::move(samplingRates))
	  , m_eegRanges(std::move(eegRanges))
	  , m_bipRanges(std::move(bipRanges)) {}

bool CConfigurationEEGO_UG::preConfigure()
{
	if (!CConfigurationBuilder::preConfigure()) { return false; }

	m_eegRangeComboBox = GTK_COMBO_BOX(gtk_builder_get_object(m_builder, "combobox_signal_range_eeg"));
	m_bipRangeComboBox = GTK_COMBO_BOX(gtk_builder_get_object(m_builder, "combobox_signal_range_bip"));

	if (!m_eegRangeComboBox) { m_driverCtx.getLogManager() << Kernel::LogLevel_Error << "Could not connect to range widget\n"; }
	else
	{
		if (m_eegoHeader.isEEGRangeSet()) { GTKComboBoxSetActiveText(m_eegRangeComboBox, std::to_string(m_eegoHeader.getEEGRange()).c_str()); }
		else { gtk_combo_box_set_active(m_eegRangeComboBox, 0); }

		if (m_eegoHeader.isBIPRangeSet()) { GTKComboBoxSetActiveText(m_bipRangeComboBox, std::to_string(m_eegoHeader.getBIPRange()).c_str()); }
		else { gtk_combo_box_set_active(m_bipRangeComboBox, 0); }
	}

	// Repopulate sampling-rate combobox from SDK if we have live data
	if (!m_samplingRates.empty() && m_sampling)
	{
		const int prevRate = m_header->getSamplingFrequency();
		GtkListStore* store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(m_sampling)));
		gtk_list_store_clear(store);
		int activeIdx = 0, idx = 0;
		for (int rate : m_samplingRates)
		{
			gtk_combo_box_append_text(GTK_COMBO_BOX(m_sampling), std::to_string(rate).c_str());
			if (rate == prevRate) { activeIdx = idx; }
			idx++;
		}
		gtk_combo_box_set_active(GTK_COMBO_BOX(m_sampling), activeIdx);
	}

	// Repopulate EEG range combobox (SDK gives volts; UI shows mV)
	if (!m_eegRanges.empty() && m_eegRangeComboBox)
	{
		const int prevEEGmV = m_eegoHeader.isEEGRangeSet() ? int(m_eegoHeader.getEEGRange()) : -1;
		GtkListStore* store = GTK_LIST_STORE(gtk_combo_box_get_model(m_eegRangeComboBox));
		gtk_list_store_clear(store);
		int activeIdx = 0, idx = 0;
		for (double rangeV : m_eegRanges)
		{
			const int mV = int(std::round(rangeV * 1000.0));
			gtk_combo_box_append_text(m_eegRangeComboBox, std::to_string(mV).c_str());
			if (mV == prevEEGmV) { activeIdx = idx; }
			idx++;
		}
		gtk_combo_box_set_active(m_eegRangeComboBox, activeIdx);
	}

	// Repopulate BIP range combobox
	if (!m_bipRanges.empty() && m_bipRangeComboBox)
	{
		const int prevBIPmV = m_eegoHeader.isBIPRangeSet() ? int(m_eegoHeader.getBIPRange()) : -1;
		GtkListStore* store = GTK_LIST_STORE(gtk_combo_box_get_model(m_bipRangeComboBox));
		gtk_list_store_clear(store);
		int activeIdx = 0, idx = 0;
		for (double rangeV : m_bipRanges)
		{
			const int mV = int(std::round(rangeV * 1000.0));
			gtk_combo_box_append_text(m_bipRangeComboBox, std::to_string(mV).c_str());
			if (mV == prevBIPmV) { activeIdx = idx; }
			idx++;
		}
		gtk_combo_box_set_active(m_bipRangeComboBox, activeIdx);
	}

	// Set the spinbutton upper bound to the actual number of channels on this amplifier
	const uint32_t totalChannels = m_nEEGChannels + m_nBIPChannels;
	if (m_nChannels)
	{
		GtkAdjustment* adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(m_nChannels));
		gtk_adjustment_set_upper(adj, double(totalChannels));
		// Default to all channels if nothing was previously configured
		if (gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(m_nChannels)) > gint(totalChannels)) {
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_nChannels), double(totalChannels));
		}
	}

	// Update hint label with actual counts
	GtkWidget* hint = GTK_WIDGET(gtk_builder_get_object(m_builder, "label_channel_hint"));
	if (hint)
	{
		std::stringstream ss;
		ss << "Slot number = hardware channel index. "
		   << "Slots 1-" << m_nEEGChannels << " are EEG, "
		   << "slots " << (m_nEEGChannels + 1) << "-" << totalChannels << " are BIP/AUX. "
		   << "Name a slot to record it; leave blank to skip. "
		   << "Use 'Change channel names' to edit, save and load your cap definition.";
		gtk_label_set_text(GTK_LABEL(hint), ss.str().c_str());
	}

	return true;
}

bool CConfigurationEEGO_UG::postConfigure()
{
	if (m_applyConfig)
	{
		const gchar* rangeEEG = gtk_combo_box_get_active_text(m_eegRangeComboBox);
		const gchar* rangeBip = gtk_combo_box_get_active_text(m_bipRangeComboBox);
		m_eegoHeader.setEEGRange(rangeEEG ? atoi(rangeEEG) : 0);
		m_eegoHeader.setBIPRange(rangeBip ? atoi(rangeBip) : 0);
	}

	// Base class populates Subject ID, Age, Gender, channel names and count, sampling frequency
	if (!CConfigurationBuilder::postConfigure()) { return false; }

	if (m_applyConfig)
	{
		// Derive EEG and BIP masks from which channel-name slots are non-empty.
		// Slots 0-63  map to EEG channels 0-63  (bits in the 64-bit EEG mask).
		// Slots 64-87 map to BIP channels 0-23  (bits in the 24-bit BIP mask).
		// The output channel list is compacted: only named slots, in index order.
		const uint32_t totalSlots = m_header->getChannelCount(); // what the user set in the spinbutton
		uint64_t eegMask = 0, bipMask = 0;
		uint32_t namedCount = 0;

		for (uint32_t i = 0; i < totalSlots; ++i)
		{
			const std::string name(m_header->getChannelName(i));
			if (name.empty()) { continue; }

			if (i < m_nEEGChannels) { eegMask |= (uint64_t(1) << i); }
			else if (i < m_nEEGChannels + m_nBIPChannels) { bipMask |= (uint64_t(1) << (i - m_nEEGChannels)); }

			// Compact: move this name to the next output position
			if (namedCount != i) { m_header->setChannelName(namedCount, name.c_str()); }
			namedCount++;
		}

		// Store masks for the driver (as hex strings)
		std::stringstream ssEEG, ssBIP;
		ssEEG << "0x" << std::hex << eegMask;
		ssBIP << "0x" << std::hex << bipMask;
		m_eegoHeader.setEEGMask(CString(ssEEG.str().c_str()));
		m_eegoHeader.setBIPMask(CString(ssBIP.str().c_str()));

		m_header->setChannelCount(namedCount);
		// Sync the hidden spinbutton the base class reads back
		if (m_nChannels) { gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_nChannels), namedCount); }
	}

	return true;
}

}  // namespace AcquisitionServer
}  // namespace OpenViBE
#endif // TARGET_HAS_ThirdPartyEEGOAPI


