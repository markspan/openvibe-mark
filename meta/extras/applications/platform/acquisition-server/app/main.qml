import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material


import OVASInterface as OV

//window containing the application
ApplicationWindow {
  id: window
  visible: true

  //title of the application
  title: qsTr("Openvibe Acquisition Manager")
  width: 640
  height: 480

  OV.OVMaster {
    id: master
  }

  //menu containing two menu items
  menuBar: MenuBar {
    Menu {
      title: qsTr("File")
      MenuItem {
        text: qsTr("&Open")
        onTriggered: console.log("Open action triggered");
      }
      MenuItem {
        text: qsTr("Exit")
        onTriggered: Qt.quit();
      }
    }
  }

  //Content Area
  ColumnLayout {
    id: mainLayout
    anchors.fill: parent

    RowLayout {
      Layout.alignment: Qt.AlignTop
      Layout.preferredWidth: parent.width*0.8
      GridLayout {
        columns: 3
        flow: GridLayout.LeftToRight
        Layout.fillHeight: true

        Text {
          text: "Driver"
        }
        ComboBox {
          id: cb_drivers
          model: master.drivers

          onCurrentTextChanged: {
            console.log("miaouuuu, ", currentText)
            //master.setCurrentDriver(currentText)
          }
        }
        Button {
          text: "Driver Properties"
          onClicked: {
            device_config.open();
          }
        }

        Text {
          text: "Connection Port"
        }
        ComboBox {
          Layout.columnSpan: 2
          Layout.fillWidth: true

          model: ["1024 ", "Second", "Third"]
        }

        Text {
          text: "Sample count per sent block"
        }
        ComboBox {
          Layout.columnSpan: 2
          Layout.fillWidth: true

          model: ["32", "Second", "Third"]
        }

      }

      ColumnLayout {
        Layout.minimumWidth: 80
        Button {
          Layout.alignment: Qt.AlignRight
          text: qsTr("Preferences")
        }
        Button {
          Layout.alignment: Qt.AlignRight
          text: qsTr("Connect")
          onClicked:  {
            if(text === "Connect") {
              text= "disconnect";
              button_play.enabled = true;
              button_stop.enabled = true;
            } else {
              text= "Connect";
              button_play.enabled = false;
              button_stop.enabled = false;
              }
          }
        }
        Button {
          id: button_play
          Layout.alignment: Qt.AlignRight
          text: qsTr("Play")
          enabled: false
        }
        Button {
          id: button_stop
          Layout.alignment: Qt.AlignRight
          text: qsTr("Stop")
          enabled: false
        }
      }
    } // RowLayout

    Text {
      id: drift_text
      Layout.alignment: Qt.AlignHCenter
      text: "Device drift: 0.00ms"
    }

    RowLayout {
      Layout.fillWidth: true
      ProgressBar {
        width: window.width/2
        value: 0.5
      }

      ProgressBar {
        width: window.width/2
        value: 0.2
      }
    }

  }

  //Dialog
  Dialog {
    id: device_config
    title: "Device Configuration"
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel

    GridLayout {
      columns: 2
      flow: GridLayout.LeftToRight


      //model: master.driverParameters

      Text { text: "Identifier" }
      SpinBox { value: 0 }
      Text { text: "Age" }
      SpinBox { value: 18 }
      Text { text: "Gender" }
      ComboBox { model: ["Other", "Male", "Female"] }
      Text { text: "Buffer host name" }
      TextInput { text: "Localhost" }
      Text { text: "Buffer port number" }
      TextInput { text: "1979" }
      Text { text: "Min nb of samples in buffer" }
      SpinBox { value: 1 }
    }

    onAccepted: console.log("Ok clicked")
    onRejected: console.log("Cancel clicked")
  }


    //a button in the middle of the content area
    //Button {
    //    text: qsTr("Hello World")
    //    anchors.horizontalCenter: parent.horizontalCenter
    //    anchors.verticalCenter: parent.verticalCenter
    //}
}
