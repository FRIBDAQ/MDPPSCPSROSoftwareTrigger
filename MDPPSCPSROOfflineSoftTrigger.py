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
    finished = pyqtSignal()

    def __init__(self, infile, outfile, trigCh, windowStart, windowWidth):
        self.infileUri = QUrl.fromLocalFile(infile)
        self.outfileUri = QUrl.fromLocalFile(outfile)
        self.trigCh = trigCh
        self.windowStart = windowStart
        self.windowWidth = windowWidth


    def run(self):
        subprocess.run([os.path.join(os.path.dirname(__file__), MDPPSCPSROSoftTrigger), self.infileUri, self.outfileUri, self.trigCh, self.windowStart, self.windowWidth])
        self.finished.emit()

class About(QtWidgets.QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)

        uic.loadUi(os.path.join(os.path.dirname(__file__), 'MDPPSCPSROOfflineSoftTrigger_about.ui'), self);

class Window(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()

        self.setWindowFlags(QtCore.Qt.WindowMinimizeButtonHint | QtCore.Qt.WindowCloseButtonHint)

        uic.loadUi(os.path.join(os.path.dirname(__file__), 'MDPPSCPSROOfflineSoftTriggerControl.ui'), self)

        self.LB_log = self.findChild(QtWidgets.QLabel, 'LB_log')

        self.LE_infile = self.findChild(QtWidgets.QLineEdit, "LE_infile")
        self.LE_outfile = self.findChild(QtWidgets.QLineEdit, "LE_outfile")
        self.PB_browseInfile = self.findChild(QtWidgets.QPushButton, "PB_browseInfile")
        self.PB_browseOutfile = self.findChild(QtWidgets.QPushButton, "PB_browseOutfile")
        self.CB_trigCh = self.findChild(QtWidgets.QComboBox, "CB_trigCh")
        self.CB_trigCh.currentTextChanged.connect(self._updateComboBoxes)
        self.CB_trigCh.currentIndexChanged.connect(self._updateTrigChLabel)
        self.LE_WS = self.findChild(QtWidgets.QLineEdit, "LE_WS")
        self.LE_WW = self.findChild(QtWidgets.QLineEdit, "LE_WW")

        PB_browseInfile.pressed.connect(self.openFileSelectionDialog(PB_browseInfile))
        PB_browseOutfile.pressed.connect(self.openFileSelectionDialog(PB_browseOutfile))
        PB_start = self.findChild(QtWidgets.QPushButton, 'PB_start')
        PB_start.pressed.connect(self.startConversion)

        PB_about = self.findChild(QtWidgets.QPushButton, "PB_about")
        PB_about.pressed.connect(self._openAboutDialog)

        self.show()


    def startConversion(self):
        self._setLog(LOG_WARNING, 'Processing....')
        self.PB_browseInfile.setEnabled(False)
        self.PB_browseOutfile.setEnabled(False)
        self.LE_trigCh.setEnabled(False)
        self.LE_WS.setEnabled(False)
        self.LE_WW.setEnabled(False)
        self.PB_start.setEnabled(False);

        self.worker = ConversionThread()
        self.worker.finished.connect(self.onFinishedConversion)
        self.worker.start()


    def onFinishedConversion(self):
        self._setLog(LOG_GOOD, 'Conversion Finised!')
        self.PB_browseInfile.setEnabled(True)
        self.PB_browseOutfile.setEnabled(True)
        self.LE_trigCh.setEnabled(True)
        self.LE_WS.setEnabled(True)
        self.LE_WW.setEnabled(True)
        self.PB_start.setEnabled(True)


    def openFileSelectionDialog(self, sender):
        options = QtWidgets.QFileDialog.Options()

        if sender == self.PB_browseInfile:
            selectedFile, _ = QtWidgets.QFileDialog.getOpenFileName(self, "File to convert", '', "evt files (*.evt)", '', options)
        else:
            selectedFile, _ = QtWidgets.QFileDialog.getSaveFileName(self, "Converted file saving to", '', "evt files (*.evt)", '', options)


        if selectedFile == '':
            self._setLog(LOG_WARNING, 'Nothing selected!')

            return
        else:
            if sender == self.PB_browseInfile:
                self.LB_infile.setText(selectedFile)
            else:
                self.LB_outfile.setText(selectedFile)

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
    app = QtWidgets.QApplication()
    window = Window()
    app.exec_()
