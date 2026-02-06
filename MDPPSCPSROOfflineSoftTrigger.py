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

import os
import subprocess
from PyQt5 import QtWidgets, QtCore, uic, QtGui
from PyQt5.QtWidgets import QApplication, QMainWindow
from PyQt5.QtCore import QThread, pyqtSignal, QUrl

LOG_GOOD = '#30C300'
LOG_WARNING = '#C28E00'
LOG_ERROR = '#C30000'

class ConversionThread(QThread):
    finished = pyqtSignal(int)

    def __init__(self, infile, outfile, trigCh, windowStart, windowWidth, cut3s, rfOn, rfCh):
        super().__init__()

        self.infileUri = QUrl.fromLocalFile(infile).toString()
        self.outfileUri = QUrl.fromLocalFile(outfile).toString()
        self.trigCh = trigCh
        self.windowStart = windowStart
        self.windowWidth = windowWidth
        self.cut3s = cut3s
        self.rfOn = rfOn
        self.rfCh = rfCh


    def run(self):
        program = os.path.join(os.path.dirname(__file__), "MDPPSCPSROSoftTrigger")
        try:
            subprocess.run([program, self.infileUri, self.outfileUri, str(self.trigCh), str(self.windowStart), str(self.windowWidth), str(int(self.cut3s)), str(self.rfCh) if self.rfOn == 1 else str(-1)], check=True)
        except subprocess.CalledProcessError as e:
            self.finished.emit(e.returncode)

        self.finished.emit(0)


class About(QtWidgets.QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)

        uic.loadUi(os.path.join(os.path.dirname(__file__), 'MDPPSCPSROOfflineSoftTrigger_about.ui'), self);

class Window(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()

        self.setWindowFlags(QtCore.Qt.WindowMinimizeButtonHint | QtCore.Qt.WindowCloseButtonHint)

        uic.loadUi(os.path.join(os.path.dirname(__file__), 'MDPPSCPSROOfflineSoftTrigger.ui'), self)

        self.LB_log = self.findChild(QtWidgets.QLabel, 'LB_log')

        self.LB_infile = self.findChild(QtWidgets.QLabel, "LB_infile")
        self.LB_outfile = self.findChild(QtWidgets.QLabel, "LB_outfile")
        self.PB_browseInfile = self.findChild(QtWidgets.QPushButton, "PB_browseInfile")
        self.PB_browseOutfile = self.findChild(QtWidgets.QPushButton, "PB_browseOutfile")
        self.CB_trigCh = self.findChild(QtWidgets.QComboBox, "CB_trigCh")
        self.CB_trigCh.currentTextChanged.connect(self._updateComboBoxes)
        self.CB_trigCh.currentIndexChanged.connect(self._updateTrigChLabel)

        self.CB_cut3s = self.findChild(QtWidgets.QCheckBox, "CB_cut3s")
        self.CB_rfOn = self.findChild(QtWidgets.QCheckBox, "CB_rfOn")
        self.CB_rfCh = self.findChild(QtWidgets.QComboBox, "CB_rfCh")

        self.CB_rfOn.stateChanged.connect(lambda state: self.CB_rfCh.setEnabled(bool(state)))
        self.CB_rfCh.setEnabled(self.CB_rfOn.isChecked())

        self.LE_WS = self.findChild(QtWidgets.QLineEdit, "LE_WS")
        self.LE_WW = self.findChild(QtWidgets.QLineEdit, "LE_WW")
        self.LE_WS.textChanged.connect(self._emptyLog)
        self.LE_WW.textChanged.connect(self._emptyLog)

        validator = QtGui.QIntValidator(0, 9999999)
        self.LE_WS.setValidator(validator)
        self.LE_WW.setValidator(validator)

        self.PB_browseInfile.clicked.connect(lambda: self.openFileSelectionDialog(self.PB_browseInfile))
        self.PB_browseOutfile.clicked.connect(lambda: self.openFileSelectionDialog(self.PB_browseOutfile))
        self.PB_start = self.findChild(QtWidgets.QPushButton, 'PB_start')
        self.PB_start.clicked.connect(self.startConversion)

        PB_about = self.findChild(QtWidgets.QPushButton, "PB_about")
        PB_about.clicked.connect(self._openAboutDialog)

        self.show()


    def startConversion(self):
        if self.LB_infile.text() == '':
            self._setLog(LOG_ERROR, 'Input file is not selected!')
            return
        elif self.LB_outfile.text() == '':
            self._setLog(LOG_ERROR, 'Output file is not specified!')
            return

        self._setLog(LOG_WARNING, 'Processing....')
        self.PB_browseInfile.setEnabled(False)
        self.PB_browseOutfile.setEnabled(False)
        self.CB_trigCh.setEnabled(False)
        self.CB_cut3s.setEnabled(False)
        self.CB_rfCh.setEnabled(False)
        self.CB_rfOn.setEnabled(False)
        self.LE_WS.setEnabled(False)
        self.LE_WW.setEnabled(False)
        self.PB_start.setEnabled(False)

        self.worker = ConversionThread(self.LB_infile.text(), self.LB_outfile.text(), self.CB_trigCh.currentIndex(), self.LE_WS.text(), self.LE_WW.text(), self.CB_cut3s.isChecked(), self.CB_rfOn.isChecked(), self.CB_rfCh.currentIndex())
        self.worker.finished.connect(self.onFinishedConversion)
        self.worker.start()


    def onFinishedConversion(self, code):
        self.PB_browseInfile.setEnabled(True)
        self.PB_browseOutfile.setEnabled(True)
        self.CB_trigCh.setEnabled(True)
        self.CB_cut3s.setEnabled(True)
        self.CB_rfCh.setEnabled(True)
        self.CB_rfOn.setEnabled(True)
        self.LE_WS.setEnabled(True)
        self.LE_WW.setEnabled(True)
        self.PB_start.setEnabled(True)

        if code == 0:
            self._setLog(LOG_GOOD, 'Conversion Finished!')
        else:
            self._setLog(LOG_ERROR, 'Error! See the terminal window for more information!')


    def openFileSelectionDialog(self, sender):
        self._emptyLog()
        options = QtWidgets.QFileDialog.Options()

        if sender == self.PB_browseInfile:
            selectedFile, _ = QtWidgets.QFileDialog.getOpenFileName(self, "File to convert", '/user/n4vault/mdpp32_sro_2025/stagearea', "evt files (*.evt)", '', options)
        else:
            selectedFile, _ = QtWidgets.QFileDialog.getSaveFileName(self, "Converted file saving to", '/user/n4vault/mdpp32_sro_2025/stagearea/conversion', "evt files (*.evt)", '', options)
            if selectedFile != '' and not selectedFile.endswith(".evt"):
                selectedFile += ".evt"


        if selectedFile == '':
            self._setLog(LOG_WARNING, 'Nothing selected!')

            return
        else:
            if sender == self.PB_browseInfile:
                self._setFilePath(self.LB_infile, selectedFile)
            else:
                self._setFilePath(self.LB_outfile, selectedFile)


    def _setFilePath(self, label, path):
        label.setText(path)
        font = label.font()
        font.setPointSizeF(20)
        label.setFont(font)

        rect = label.contentsRect()
        font = label.font()
        size = font.pointSize()
        metric = QtGui.QFontMetrics(font)
        rect2 = metric.boundingRect(rect, QtCore.Qt.TextWordWrap, path)
        step = -0.01 if rect2.height() > rect.height() else 0.01
        while True:
            font.setPointSizeF(size + step)
            metric = QtGui.QFontMetrics(font)
            rect2 = metric.boundingRect(rect, QtCore.Qt.TextWordWrap, path)
            if size <= 0.1:
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
        label.setFont(font)


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

    def _emptyLog(self):
        self._setLog(LOG_WARNING, '')


    def _updateComboBoxes(self, value):
        self.CB_trigCh.currentTextChanged.disconnect()
        self.CB_trigCh.setItemText(self.sender().currentIndex(), value)
        self.CB_trigCh.currentTextChanged.connect(self._updateComboBoxes)


    def _updateTrigChLabel(self, value):
        self._emptyLog()
        self.LB_trigCh.setText(f"Trigger Channel {value}:")


    def _openAboutDialog(self):
        About(self).exec_()
        

if __name__ == "__main__":
    app = QtWidgets.QApplication([])
    window = Window()
    app.exec_()
