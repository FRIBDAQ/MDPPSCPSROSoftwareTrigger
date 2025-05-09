#!/usr/bin/python3

#    This software is Copyright by the Board of Trustees of Michigan
#    State University (c) Copyright 2025.
#
#    You may use this software under the terms of the GNU public license
#    (GPL).  The terms of this license are described at:
#
#    http://www.gnu.org/licenses/gpl.txt
#
#    Author:
#    Genie Jhang
#	   FRIB
#	   Michigan State University
#	   East Lansing, MI 48824-1321

import sys, os
from PyQt5 import QtWidgets, QtCore, uic, QtGui
from PyQt5.QtWidgets import QApplication, QMainWindow
import argparse, json

LOG_GOOD = '#30C300'
LOG_WARNING = '#C28E00'
LOG_ERROR = '#C30000'

class About(QtWidgets.QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)

        uic.loadUi(os.path.join(os.path.dirname(__file__), 'MDPPSCPSROSoftTriggerControl_about.ui'), self);

class Window(QtWidgets.QMainWindow):
    def __init__(self, data):
        super().__init__()

        self.setWindowFlags(QtCore.Qt.WindowMinimizeButtonHint | QtCore.Qt.WindowCloseButtonHint)

        uic.loadUi(os.path.join(os.path.dirname(__file__), 'MDPPSCPSROSoftTriggerControl.ui'), self)

        self.LB_log = self.findChild(QtWidgets.QLabel, 'LB_log')

        self.LE_inring = self.findChild(QtWidgets.QLineEdit, "LE_inring")
        self.LE_outring = self.findChild(QtWidgets.QLineEdit, "LE_outring")
        self.CB_trigCh = self.findChild(QtWidgets.QComboBox, "CB_trigCh")
        self.CB_trigCh.currentTextChanged.connect(self._updateComboBoxes)
        self.CB_trigCh.currentIndexChanged.connect(self._updateTrigChLabel)
        self.LE_WS = self.findChild(QtWidgets.QLineEdit, "LE_WS")
        self.LE_WW = self.findChild(QtWidgets.QLineEdit, "LE_WW")

        PB_apply = self.findChild(QtWidgets.QPushButton, 'PB_apply')
        PB_load = self.findChild(QtWidgets.QPushButton, 'PB_load')
        PB_save = self.findChild(QtWidgets.QPushButton, 'PB_save')

        PB_apply.pressed.connect(self.applyToConfig)
        PB_load.pressed.connect(self.openLoadfileDialog)
        PB_save.pressed.connect(self.openSavefileDialog)

        if data != None:
            self._updateFromJson(data)

        PB_about = self.findChild(QtWidgets.QPushButton, "PB_about")
        PB_about.pressed.connect(self._openAboutDialog)

        self.show()


    def applyToConfig(self):
        inring = self.LE_inring.text()
        outring = self.LE_outring.text()
        trigCh = self.CB_trigCh.currentIndex()
        windowStart = self.LE_WS.text()
        windowWidth = self.LE_WW.text()

        with open(os.path.join(os.path.dirname(__file__), "MDPPSCPSROSoftTriggerSettings.tcl"), "w") as file:
          file.write(f"set inring {inring}\n")
          file.write(f"set outring {outring}\n")
          file.write(f"set trigCh {trigCh}\n")
          file.write(f"set windowStart {windowStart}\n")
          file.write(f"set windowWidth {windowWidth}")

        self._setLog(LOG_GOOD, 'Successfully applied the settings!')


    def openLoadfileDialog(self):
        options = QtWidgets.QFileDialog.Options()

        selectedFile, _ = QtWidgets.QFileDialog.getOpenFileName(self, "Load settings to file", '', "JSON files (*.json)", '', options)

        if selectedFile == '':
            self._setLog(LOG_WARNING, 'No changes made!')

            return
        else:
            with open(selectedFile) as file:
                data = json.load(file)

            self._updateFromJson(data)
            self._setLog(LOG_GOOD, 'Successfully loaded the file! - %s (Not applied yet)' % selectedFile.split('/')[-1])


    def openSavefileDialog(self):
        options = QtWidgets.QFileDialog.Options()

        inring = self.LE_inring.text()
        outring = self.LE_outring.text()

        trigCh = self.CB_trigCh.currentIndex()
        chNames = []
        for i in range(0, 32):
            chNames.append(self.CB_trigCh.itemText(i))

        windowStart = self.LE_WS.text()
        windowWidth = self.LE_WW.text()

        dataDict = {"inring":inring,"outring":outring,"trigCh":trigCh,"chNames":chNames,"windowStart":windowStart,"windowWidth":windowWidth}

        selectedFile, _ = QtWidgets.QFileDialog.getSaveFileName(self, "Save settings to file", '', "JSON files (*.json)", '', options)
        if selectedFile == '':
            self._setLog(LOG_WARNING, 'Nothing saved!')

            return
        else:
            if not selectedFile.lower().endswith('.json'):
                selectedFile += '.json'

            with open(selectedFile, "w") as file:
                json.dump(dataDict, file, indent=2)

            self._setLog(LOG_GOOD, 'Successfully saved to file! - %s' % selectedFile.split('/')[-1])


    def _updateFromJson(self, data):
        self.LE_inring.setText(data['inring']);
        self.LE_outring.setText(data['outring']);

        for i in range(len(data['chNames'])):
            self.CB_trigCh.setItemText(i, data['chNames'][i])

        self.CB_trigCh.setCurrentIndex(int(str(data['trigCh'])));

        self.LE_WS.setText(data['windowStart']);
        self.LE_WW.setText(data['windowWidth']);


    def _setLog(self, level, text):
        self.LB_log.setStyleSheet('color: %s;' % level)
        self.LB_log.setText(text)
        font = self.LB_log.font()
        font.setPointSizeF(20)
        self.LB_log.setFont(font)
        
        rect = self.LB_log.contentsRect()
        font = self.LB_log.font()
        size = font.pointSize()
        metric = QtGui.QFontMetrics(font)
        rect2 = metric.boundingRect(rect, QtCore.Qt.TextWordWrap, text)
        step = -0.01 if rect2.height() > rect.height() else 0.01
        while True:
            font.setPointSizeF(size + step)
            metric = QtGui.QFontMetrics(font)
            rect2 = metric.boundingRect(rect, QtCore.Qt.TextWordWrap, text)
            if size <= 1:
                break
            if step < 0:
                size += step
                if rect2.height() < rect.height():
                    break
            else:
                if rect2.height() > rect.height():
                    break
                size += step
        
        font.setPointSizeF(size)
        self.LB_log.setFont(font)


    def _updateComboBoxes(self, value):
        self.CB_trigCh.currentTextChanged.disconnect()
        self.CB_trigCh.setItemText(self.sender().currentIndex(), value)
        self.CB_trigCh.currentTextChanged.connect(self._updateComboBoxes)


    def _updateTrigChLabel(self, value):
        self.LB_trigCh.setText(f"Trigger Channel {value}:")


    def _openAboutDialog(self):
        About(self).exec_()
        

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='MDPP SCP SRO Software Trigger Control GUI launcher')
    parser.add_argument('-f', '-file',   metavar='file', dest='settingData', type=argparse.FileType('r'),
                        help='JSON setting file (optional)');

    args = parser.parse_args()
    if args.settingData != None:
        settingData = json.load(args.settingData)
    else:
        settingData = None

    app = QtWidgets.QApplication(sys.argv)
    window = Window(settingData)
    app.exec_()
