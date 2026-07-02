#include "ovasCConfigurationBuilder.h"
#include "ovasIHeader.h"

#include <toolkit/ovtk_all.h>

#include <iostream>
#include <fstream>
#include <list>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <iterator>
#include <map>

#define OVAS_ElectrodeNames_File           OpenViBE::Directories::getDataDir() + "/applications/acquisition-server/electrode-names.txt"
#define OVAS_ConfigureGUIElectrodes_File   OpenViBE::Directories::getDataDir() + "/applications/acquisition-server/interface-channel-names.ui"

namespace OpenViBE {
namespace AcquisitionServer {

static void GTKComboBoxSetActiveText(GtkComboBox* box, const gchar* sActiveText)
{
	GtkTreeModel* treeModel = gtk_combo_box_get_model(box);
	GtkTreeIter it;
	int index   = 0;
	gchar* name = nullptr;
	if (gtk_tree_model_get_iter_first(treeModel, &it)) {
		do {
			gtk_tree_model_get(treeModel, &it, 0, &name, -1);
			if (std::string(name) == std::string(sActiveText)) {
				gtk_combo_box_set_active(box, index);
				return;
			}
			index++;
		} while (gtk_tree_model_iter_next(treeModel, &it));
	}
}

// Minimal parser for a Charmeleon montage file:
//   { "<name>": { ..., "IsActive": true, "LabelText": "Cz", "HardwareChannel": 16, ... }, ... }
// Returns HardwareChannel (1-based) -> electrode name for the ACTIVE electrodes only
// ("IsActive": true); inactive electrodes are dropped here so they never reach a slot.
// Robust to key order; entry objects are flat (all scalar fields), which the Charmeleon
// export guarantees.
static std::map<int, std::string> parseCharmeleonMontage(const std::string& s)
{
	std::map<int, std::string> result;
	const size_t n = s.size();
	size_t i       = 0;

	auto skipWS = [&]() { while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) { ++i; } };

	auto readString = [&](std::string& out) -> bool {
		skipWS();
		if (i >= n || s[i] != '"') { return false; }
		++i;
		out.clear();
		while (i < n && s[i] != '"') { out += s[i++]; }
		if (i < n && s[i] == '"') { ++i; return true; }
		return false;
	};

	skipWS();
	if (i >= n || s[i] != '{') { return result; }
	++i;

	while (true) {
		skipWS();
		if (i < n && s[i] == '}') { ++i; break; }

		std::string key;
		if (!readString(key)) { break; }
		skipWS();
		if (i < n && s[i] == ':') { ++i; }
		skipWS();
		if (i >= n || s[i] != '{') { break; }
		++i;

		std::string label;
		int channel    = 0;
		bool haveLabel = false, haveChannel = false, isActive = false;

		while (true) {
			skipWS();
			if (i < n && s[i] == '}') { ++i; break; }

			std::string field;
			if (!readString(field)) { break; }
			skipWS();
			if (i < n && s[i] == ':') { ++i; }
			skipWS();

			if (field == "LabelText") { if (readString(label)) { haveLabel = true; } }
			else if (field == "HardwareChannel") {
				std::string num;
				while (i < n && (std::isdigit(static_cast<unsigned char>(s[i])) || s[i] == '-')) { num += s[i++]; }
				if (!num.empty()) { channel = std::atoi(num.c_str()); haveChannel = true; }
			}
			else if (field == "IsActive") {
				// JSON boolean literal; only an explicit "true" marks the electrode active
				std::string lit;
				while (i < n && std::isalpha(static_cast<unsigned char>(s[i]))) { lit += s[i++]; }
				isActive = (lit == "true");
			}
			else {
				if (i < n && s[i] == '"') { std::string dummy; readString(dummy); }
				else { while (i < n && s[i] != ',' && s[i] != '}') { ++i; } }
			}

			skipWS();
			if (i < n && s[i] == ',') { ++i; }
		}

		// Admit an electrode only when it is active and carries a valid 1-based hardware
		// channel. The channel-in-amplifier-range check happens where the map is applied
		// (the channel-name rows span exactly the amplifier's channels).
		if (haveLabel && haveChannel && channel > 0 && isActive) { result[channel] = label; }

		skipWS();
		if (i < n && s[i] == ',') { ++i; }
	}

	return result;
}

static void button_change_channel_names_cb(GtkButton* /*button*/, void* data)
{
#if defined _DEBUG_Callbacks_
	std::cout << "button_change_channel_names_cb" << std::endl;
#endif
	static_cast<CConfigurationBuilder*>(data)->buttonChangeChannelNamesCB();
}

static void button_apply_channel_name_cb(GtkButton* /*button*/, void* data)
{
#if defined _DEBUG_Callbacks_
	std::cout << "button_apply_channel_name_cb" << std::endl;
#endif
	static_cast<CConfigurationBuilder*>(data)->buttonApplyChannelNameCB();
}

static void button_remove_channel_name_cb(GtkButton* /*button*/, void* data)
{
#if defined _DEBUG_Callbacks_
	std::cout << "button_remove_channel_name_cb" << std::endl;
#endif
	static_cast<CConfigurationBuilder*>(data)->buttonRemoveChannelNameCB();
}

static void treeview_apply_channel_name_cb(GtkTreeView* /*treeview*/, GtkTreePath* /*path*/, GtkTreeViewColumn* /*column*/, void* data)
{
#if defined _DEBUG_Callbacks_
	std::cout << "treeview_apply_channel_name_cb" << std::endl;
#endif
	static_cast<CConfigurationBuilder*>(data)->treeviewApplyChannelNameCB();
}

//___________________________________________________________________//
//                                                                   //

CConfigurationBuilder::CConfigurationBuilder(const char* gtkBuilderFilename)
	: m_gtkBuilderFilename(gtkBuilderFilename ? gtkBuilderFilename : ""),
	  m_electrodeFilename(OVAS_ElectrodeNames_File),
	  m_gtkBuilderChannelsFilename(OVAS_ConfigureGUIElectrodes_File) {}

CConfigurationBuilder::~CConfigurationBuilder() {}

//___________________________________________________________________//
//                                                                   //

bool CConfigurationBuilder::configure(IHeader& header)
{
	m_applyConfig = true;

	m_header = &header;
	if (!this->preConfigure()) {
		std::cout << "Error: Driver preconfigure failed\n";
		m_applyConfig = false;
	}

	// Only run if preConfig succeeded
	EError code;
	if (m_applyConfig && !this->doConfigure(code)) {
		if (code != EError::UserCancelled) { std::cout << "Note: Driver doConfigure failed with code " << toString(code) << "\n"; }
		m_applyConfig = false;
	}

	// Run the postconfigure in any case to close a possible dialog
	if (!this->postConfigure()) {
		std::cout << "Error: Driver postconfigure failed\n";
		m_applyConfig = false;
	}
	m_header = nullptr;

	return m_applyConfig;
}

bool CConfigurationBuilder::preConfigure()
{
	// Prepares interface
	m_builder     = gtk_builder_new(); // glade_xml_new(m_gtkBuilderFilename.c_str(), nullptr, nullptr);
	GError* error = nullptr;
	if (!gtk_builder_add_from_file(m_builder, m_gtkBuilderFilename.c_str(), &error)) {
		std::cout << "Error: Unable to load interface from " << m_gtkBuilderFilename << ", code " << error->code << " (" << error->message << ")\n";
		return false;
	}
	m_builderChannel = gtk_builder_new(); // glade_xml_new(m_gtkBuilderChannelsFilename.c_str(), nullptr, nullptr);
	if (!gtk_builder_add_from_file(m_builderChannel, m_gtkBuilderChannelsFilename.c_str(), &error)) {
		std::cout << "Error: Unable to load channels from " << m_gtkBuilderChannelsFilename << ", code " << error->code << " (" << error->message << ")\n";
		return false;
	}

	// Finds all the widgets
	m_dialog = GTK_WIDGET(gtk_builder_get_object(m_builder, "openvibe-acquisition-server-settings"));
	if (!m_dialog) {
		std::cout << "Error: Unable to find even the basic settings dialog from " << m_gtkBuilderFilename << "\n";
		return false;
	}
	m_ID        = GTK_WIDGET(gtk_builder_get_object(m_builder, "spinbutton_identifier"));
	m_age       = GTK_WIDGET(gtk_builder_get_object(m_builder, "spinbutton_age"));
	m_nChannels = GTK_WIDGET(gtk_builder_get_object(m_builder, "spinbutton_number_of_channels"));
	// NB: sampling_frequency is not really a "combobox" everywhere (eg telnet reader), should update in every UI the ID name
	m_sampling = GTK_WIDGET(gtk_builder_get_object(m_builder, "combobox_sampling_frequency"));
	m_gender   = GTK_WIDGET(gtk_builder_get_object(m_builder, "combobox_gender"));
	if (gtk_builder_get_object(m_builder, "checkbutton_impedance")) {
		m_impedanceCheck = GTK_WIDGET(gtk_builder_get_object(m_builder, "checkbutton_impedance"));
	}
	m_electrodeNameTreeView = GTK_WIDGET(gtk_builder_get_object(m_builderChannel, "treeview_electrode_names"));
	m_channelNameTreeView   = GTK_WIDGET(gtk_builder_get_object(m_builderChannel, "treeview_channel_names"));

	// Prepares electrode name tree view
	GtkTreeView* electrodeTreeView             = GTK_TREE_VIEW(m_electrodeNameTreeView);
	GtkCellRenderer* electrodeIdxCellRenderer  = gtk_cell_renderer_text_new();
	GtkTreeViewColumn* electrodeIdxTreeViewCol = gtk_tree_view_column_new_with_attributes("Name", electrodeIdxCellRenderer, "text", 0, nullptr);

	gtk_tree_view_append_column(electrodeTreeView, electrodeIdxTreeViewCol);

	// Prepares channel name tree view
	GtkTreeView* channelTreeView                      = GTK_TREE_VIEW(m_channelNameTreeView);
	GtkCellRenderer* channelIdxCellRenderer           = gtk_cell_renderer_text_new();
	GtkCellRenderer* channelValueCellRenderer         = gtk_cell_renderer_text_new();
	GtkTreeViewColumn* channelIdxTreeViewCol          = gtk_tree_view_column_new_with_attributes("Index", channelIdxCellRenderer, "text", 0, nullptr);
	GtkTreeViewColumn* channelNameValueTreeViewColumn = gtk_tree_view_column_new_with_attributes("Name", channelValueCellRenderer, "text", 1, nullptr);
	gtk_tree_view_append_column(channelTreeView, channelIdxTreeViewCol);
	gtk_tree_view_append_column(channelTreeView, channelNameValueTreeViewColumn);

	// Connects custom GTK signals
	GObject* tmp[4];
	tmp[0] = gtk_builder_get_object(m_builder, "button_change_channel_names");
	tmp[1] = gtk_builder_get_object(m_builderChannel, "button_apply_channel_name");
	tmp[2] = gtk_builder_get_object(m_builderChannel, "button_remove_channel_name");
	tmp[3] = gtk_builder_get_object(m_builderChannel, "treeview_electrode_names");
	if (tmp[0]) { g_signal_connect(tmp[0], "pressed", G_CALLBACK(button_change_channel_names_cb), this); }
	if (tmp[1]) { g_signal_connect(tmp[1], "pressed", G_CALLBACK(button_apply_channel_name_cb), this); }
	if (tmp[2]) { g_signal_connect(tmp[2], "pressed", G_CALLBACK(button_remove_channel_name_cb), this); }
	if (tmp[3]) { g_signal_connect(tmp[3], "row-activated", G_CALLBACK(treeview_apply_channel_name_cb), this); }
	if (!tmp[0] || !tmp[1] || !tmp[2] || !tmp[3]) {
		// @fixme should make a log entry if any NULL but no manager here
#if defined _DEBUG_Callbacks_
		std::cout << "Note: The driver UI file lacks some of the expected buttons or elements. This may be intentional." << std::endl;
#endif
	}
	gtk_builder_connect_signals(m_builder, nullptr);
	gtk_builder_connect_signals(m_builderChannel, nullptr);

	// Add a "Load from Charmeleon montage" action to the channel-names dialog
	// (response id 3, handled in buttonChangeChannelNamesCB). Placed first so it
	// sits beside the existing Load.../Save... buttons.
	GtkWidget* channelDialog = GTK_WIDGET(gtk_builder_get_object(m_builderChannel, "channel-names"));
	if (channelDialog) {
		GtkWidget* loadMontageBtn = gtk_dialog_add_button(GTK_DIALOG(channelDialog), "Montage...", 3);
		GtkWidget* actionArea     = gtk_dialog_get_action_area(GTK_DIALOG(channelDialog));
		if (actionArea && loadMontageBtn) { gtk_box_reorder_child(GTK_BOX(actionArea), loadMontageBtn, 0); }
	}

	// Configures interface with preconfigured values

	if (m_header->isExperimentIDSet()) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_ID), m_header->getExperimentID());
	}

	if (m_header->isSubjectAgeSet()) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_age), m_header->getSubjectAge());
	}

	if (m_header->isChannelCountSet()) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_nChannels), m_header->getChannelCount());
	}

	if (m_header->isSamplingFrequencySet()) {
		const std::string sampling = std::to_string(m_header->getSamplingFrequency());
		// sampling frequency could be set to an Int through a spin button instead of regular combo box (eg: telnet reader)
		if (std::strcmp(G_OBJECT_TYPE_NAME(m_sampling), "GtkSpinButton") == 0) {
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_sampling), m_header->getSamplingFrequency());
		}
		// would be a "GtkComboBox"
		else {
			GTKComboBoxSetActiveText(GTK_COMBO_BOX(m_sampling), sampling.c_str());
		}
	}
	else {
		// sampling frequency could be set to an Int through a spin button instead of regular combo box (eg: telnet reader)
		if (std::strcmp(G_OBJECT_TYPE_NAME(m_sampling), "GtkSpinButton") == 0) {
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(m_sampling), 0);
		}
		// would be a "GtkComboBox"
		else {
			gtk_combo_box_set_active(GTK_COMBO_BOX(m_sampling), 0);
		}
	}
	if (m_header->isSubjectGenderSet()) {
		GTKComboBoxSetActiveText(GTK_COMBO_BOX(m_gender), m_header->getSubjectGender() == OVTK_Value_Gender_Male ? "male"
															  : m_header->getSubjectGender() == OVTK_Value_Gender_Female ? "female"
																	: m_header->getSubjectGender() == OVTK_Value_Gender_Unknown ? "unknown" : "unspecified");
	}
	else {
		gtk_combo_box_set_active(GTK_COMBO_BOX(m_gender), 0);
	}
	if (m_impedanceCheck) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_impedanceCheck), m_header->isImpedanceCheckRequested());
	}

	// Prepares channel name cache
	if (m_header->isChannelCountSet()) {
		for (size_t i = 0; i < m_header->getChannelCount(); ++i) {
			m_channelNames[i] = m_header->getChannelName(i);
		}
	}
	return true;
}

bool CConfigurationBuilder::doConfigure(EError& errorCode)
{
	if (!m_dialog) {
		errorCode = EError::Unknown;
		return false;
	}

	const gint response = gtk_dialog_run(GTK_DIALOG(m_dialog));

	switch (response) {
		case GTK_RESPONSE_APPLY: errorCode = EError::NoError;
			return true;
		case GTK_RESPONSE_CANCEL: errorCode = EError::UserCancelled;
			return false;
		default: errorCode = EError::Unknown;
			return false;
	}
}

bool CConfigurationBuilder::postConfigure()
{
	if (m_applyConfig) {
		gtk_spin_button_update(GTK_SPIN_BUTTON(m_ID));
		gtk_spin_button_update(GTK_SPIN_BUTTON(m_age));
		gtk_spin_button_update(GTK_SPIN_BUTTON(m_nChannels));

		const std::string gender = gtk_combo_box_get_active_text(GTK_COMBO_BOX(m_gender));
		m_header->setExperimentID(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(m_ID)));
		m_header->setSubjectAge(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(m_age)));
		m_header->setChannelCount(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(m_nChannels)));
		// sampling frequency could be set to an Int through a spin button instead of regular combo box (eg: telnet reader)
		if (std::strcmp(G_OBJECT_TYPE_NAME(m_sampling), "GtkSpinButton") == 0) {
			gtk_spin_button_update(GTK_SPIN_BUTTON(m_sampling));
			m_header->setSamplingFrequency(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(m_sampling)));
		}
		// would be a "GtkComboBox"
		else {
			gchar* sampling = gtk_combo_box_get_active_text(GTK_COMBO_BOX(m_sampling));
			m_header->setSamplingFrequency(sampling ? atoi(sampling) : 0);
		}
		m_header->setSubjectGender(gender == "male" ? OVTK_Value_Gender_Male
									   : gender == "female" ? OVTK_Value_Gender_Female
											 : gender == "unknown" ? OVTK_Value_Gender_Unknown : OVTK_Value_Gender_NotSpecified);
		for (size_t i = 0; i != m_header->getChannelCount(); ++i) { m_header->setChannelName(i, m_channelNames[i].c_str()); }

		if (m_impedanceCheck) { m_header->setImpedanceCheckRequested(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_impedanceCheck)) != FALSE); }
		else { m_header->setImpedanceCheckRequested(false); }
	}

	if (m_dialog) { gtk_widget_hide(m_dialog); }

	if (m_builder) {
		g_object_unref(m_builder);
		m_builder = nullptr;
	}

	if (m_builderChannel) {
		g_object_unref(m_builderChannel);
		m_builderChannel = nullptr;
	}

	m_channelNames.clear();

	return true;
}

void CConfigurationBuilder::buttonChangeChannelNamesCB()
{
	GtkTreeIter itElectrodeName, itChannelName;
	GtkDialog* dialog                  = GTK_DIALOG(gtk_builder_get_object(m_builderChannel, "channel-names"));
	GtkTreeView* electrodeNameTreeView = GTK_TREE_VIEW(m_electrodeNameTreeView);
	GtkTreeView* channelNameTreeView   = GTK_TREE_VIEW(m_channelNameTreeView);

	// Creates electrode name and channel name models

	m_electrodeNameListStore = gtk_list_store_new(1, G_TYPE_STRING);
	m_channelNameListStore   = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

	// Fills in electrode name model
	{
		std::ifstream file(m_electrodeFilename.c_str());
		if (file.is_open()) {
			std::list<std::string> electrodNames;
			while (!file.eof()) {
				std::string name;
				file >> name;
				electrodNames.push_back(name);
			}
			file.close();

			for (auto it = electrodNames.begin(); it != electrodNames.end(); ++it) {
				gtk_list_store_append(m_electrodeNameListStore, &itElectrodeName);
				gtk_list_store_set(m_electrodeNameListStore, &itElectrodeName, 0, it->c_str(), -1);
			}
		}
	}
	// Fills in channel name model

	const uint32_t nChannel = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(m_nChannels));
	for (size_t i = 0; i < nChannel; ++i) {
		std::string name = std::to_string(i + 1);
		gtk_list_store_append(m_channelNameListStore, &itChannelName);
		gtk_list_store_set(m_channelNameListStore, &itChannelName, 0, name.c_str(), -1);
		gtk_list_store_set(m_channelNameListStore, &itChannelName, 1, m_channelNames[i].c_str(), -1);
	}

	// Attachs model to views

	gtk_tree_view_set_model(electrodeNameTreeView, GTK_TREE_MODEL(m_electrodeNameListStore));
	gtk_tree_view_set_model(channelNameTreeView, GTK_TREE_MODEL(m_channelNameListStore));

	// Allow selecting multiple electrode names at once so they can be applied as a block
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(electrodeNameTreeView), GTK_SELECTION_MULTIPLE);

	// Selects first line of each
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_electrodeNameListStore), &itElectrodeName)) {
		gtk_tree_selection_select_iter(gtk_tree_view_get_selection(electrodeNameTreeView), &itElectrodeName);
	}
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName)) {
		gtk_tree_selection_select_iter(gtk_tree_view_get_selection(channelNameTreeView), &itChannelName);
	}

	// Runs dialog !

	gint dialogResponse;
	do {
		dialogResponse = gtk_dialog_run(dialog);
		switch (dialogResponse) {
			case GTK_RESPONSE_APPLY:
			{
				size_t i           = 0;
				gchar* channelName = nullptr;
				if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName)) {
					do {
						gtk_tree_model_get(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName, 1, &channelName, -1);
						m_channelNames[i++] = channelName;
					} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName));
				}
			}
			break;

			case 1: // Load
			{
				GtkWidget* widgetDialogOpen = gtk_file_chooser_dialog_new("Select file to open...", nullptr, GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL,
																		  GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, nullptr);
				//gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(widgetDialogOpen), this->getWorkingDirectory().toASCIIString());
				if (gtk_dialog_run(GTK_DIALOG(widgetDialogOpen)) == GTK_RESPONSE_ACCEPT) {
					char* fileName = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widgetDialogOpen));
					std::ifstream file(fileName);
					if (file.is_open()) {
						std::list<std::string> electrodNames;
						// Reads channel names from file
						std::string name;
						while (getline(file, name)) { electrodNames.push_back(name); }
						file.close();

						// Clears list store
						if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName)) {
							do { gtk_list_store_set(m_channelNameListStore, &itChannelName, 1, "", -1); } while (gtk_tree_model_iter_next(
								GTK_TREE_MODEL(m_channelNameListStore), &itChannelName));
						}

						// Fills list store with channel names
						auto it = electrodNames.begin();
						if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName) && it != electrodNames.end()) {
							do {
								gtk_list_store_set(m_channelNameListStore, &itChannelName, 1, it->c_str(), -1);
								++it;
							} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName) && it != electrodNames.end());
						}
					}
					g_free(fileName);
				}
				gtk_widget_destroy(widgetDialogOpen);
			}
			break;

			case 2: // Save
			{
				GtkWidget* widgetDialogOpen = gtk_file_chooser_dialog_new("Select file to save to...", nullptr, GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL,
																		  GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, nullptr);
				//gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(widgetDialogOpen), this->getWorkingDirectory().toASCIIString());
				if (gtk_dialog_run(GTK_DIALOG(widgetDialogOpen)) == GTK_RESPONSE_ACCEPT) {
					char* fileName = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widgetDialogOpen));
					std::ofstream file(fileName);
					if (file.is_open()) {
						gchar* channelName = nullptr;
						if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName)) {
							do {
								gtk_tree_model_get(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName, 1, &channelName, -1);
								file << channelName << "\n";
							} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName));
						}
					}
					g_free(fileName);
				}
				gtk_widget_destroy(widgetDialogOpen);
			}
			break;
			case 3: // Load channel names from a Charmeleon montage (name -> HardwareChannel)
			{
				GtkWidget* montageChooser = gtk_file_chooser_dialog_new("Select Charmeleon montage (*.json)...", nullptr, GTK_FILE_CHOOSER_ACTION_OPEN,
																	GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, nullptr);
				GtkFileFilter* jsonFilter = gtk_file_filter_new();
				gtk_file_filter_set_name(jsonFilter, "Charmeleon montage (*.json)");
				gtk_file_filter_add_pattern(jsonFilter, "*.json");
				gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(montageChooser), jsonFilter);
				if (gtk_dialog_run(GTK_DIALOG(montageChooser)) == GTK_RESPONSE_ACCEPT) {
					char* fileName = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(montageChooser));
					std::ifstream file(fileName);
					if (file.is_open()) {
						const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
						file.close();
						const std::map<int, std::string> montage = parseCharmeleonMontage(text);
						// Charmeleon HardwareChannel is 1-based; row r (0-based) shows as channel r+1.
						// The row set spans exactly the amplifier's channels, so a montage entry
						// whose hardware channel is out of range has no row and is silently dropped:
						// only active electrodes with an in-range hardware channel land in a slot.
						size_t r = 0;
						if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName)) {
							do {
								const auto it = montage.find(int(r) + 1);
								gtk_list_store_set(m_channelNameListStore, &itChannelName, 1, it != montage.end() ? it->second.c_str() : "", -1);
								++r;
							} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName));
						}
					}
					g_free(fileName);
				}
				gtk_widget_destroy(montageChooser);
			}
			break;
			default: break;
		}
	} while (dialogResponse != GTK_RESPONSE_APPLY && dialogResponse != GTK_RESPONSE_CANCEL);

	gtk_widget_hide(GTK_WIDGET(dialog));
	g_object_unref(m_channelNameListStore);
	g_object_unref(m_electrodeNameListStore);
	m_channelNameListStore   = nullptr;
	m_electrodeNameListStore = nullptr;
}

void CConfigurationBuilder::buttonApplyChannelNameCB()
{
	GtkTreeIter itChannelName;
	GtkTreeView* electrodeNameTreeView = GTK_TREE_VIEW(m_electrodeNameTreeView);
	GtkTreeView* channelNameTreeView   = GTK_TREE_VIEW(m_channelNameTreeView);

	GtkTreeSelection* channelSel   = gtk_tree_view_get_selection(channelNameTreeView);
	GtkTreeSelection* electrodeSel = gtk_tree_view_get_selection(electrodeNameTreeView);

	// Ensure a starting row is selected on the right
	if (!gtk_tree_selection_get_selected(channelSel, nullptr, &itChannelName)) {
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_channelNameListStore), &itChannelName)) {
			gtk_tree_selection_select_iter(channelSel, &itChannelName);
		}
	}
	if (!gtk_tree_selection_get_selected(channelSel, nullptr, &itChannelName)) { return; }

	// Collect selected electrode rows in model order
	GtkTreeModel* electrodeModel = GTK_TREE_MODEL(m_electrodeNameListStore);
	GList* selectedPaths         = gtk_tree_selection_get_selected_rows(electrodeSel, &electrodeModel);
	if (!selectedPaths) { return; }

	// Sort by path so they are applied top-to-bottom regardless of selection order
	selectedPaths = g_list_sort(selectedPaths, reinterpret_cast<GCompareFunc>(gtk_tree_path_compare));

	for (GList* node = selectedPaths; node != nullptr; node = node->next) {
		GtkTreeIter itElec;
		if (!gtk_tree_model_get_iter(electrodeModel, &itElec, static_cast<GtkTreePath*>(node->data))) { continue; }

		gchar* name = nullptr;
		gtk_tree_model_get(electrodeModel, &itElec, 0, &name, -1);
		gtk_list_store_set(m_channelNameListStore, &itChannelName, 1, name, -1);

		// Advance the right-panel selection; stay on last row if we run out
		GtkTreeIter next = itChannelName;
		if (gtk_tree_model_iter_next(GTK_TREE_MODEL(m_channelNameListStore), &next)) {
			itChannelName = next;
		}
	}

	// Leave the right-panel selection at the row after the last one filled
	gtk_tree_selection_unselect_all(channelSel);
	gtk_tree_selection_select_iter(channelSel, &itChannelName);

	g_list_free_full(selectedPaths, reinterpret_cast<GDestroyNotify>(gtk_tree_path_free));
}

void CConfigurationBuilder::buttonRemoveChannelNameCB()
{
	GtkTreeIter it;
	GtkTreeView* treeView = GTK_TREE_VIEW(m_channelNameTreeView);

	GtkTreeSelection* selection = gtk_tree_view_get_selection(treeView);
	if (gtk_tree_selection_get_selected(selection, nullptr, &it)) {
		gtk_list_store_set(m_channelNameListStore, &it, 1, "", -1);
		if (gtk_tree_model_iter_next(GTK_TREE_MODEL(m_channelNameListStore), &it)) { gtk_tree_selection_select_iter(selection, &it); }
		else { if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(m_channelNameListStore), &it)) { gtk_tree_selection_select_iter(selection, &it); } }
	}
}

void CConfigurationBuilder::treeviewApplyChannelNameCB() { this->buttonApplyChannelNameCB(); }

}  // namespace AcquisitionServer
}  // namespace OpenViBE
