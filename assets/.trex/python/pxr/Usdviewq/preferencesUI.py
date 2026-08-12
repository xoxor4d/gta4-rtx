# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'preferencesUI.ui'
##
## Created by: Qt User Interface Compiler version 6.7.3
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QBrush, QColor, QConicalGradient, QCursor,
    QFont, QFontDatabase, QGradient, QIcon,
    QImage, QKeySequence, QLinearGradient, QPainter,
    QPalette, QPixmap, QRadialGradient, QTransform)
from PySide6.QtWidgets import (QAbstractButton, QApplication, QDialog, QDialogButtonBox,
    QFrame, QHBoxLayout, QLabel, QSizePolicy,
    QSpacerItem, QSpinBox, QVBoxLayout, QWidget)

class Ui_Preferences(object):
    def setupUi(self, Preferences):
        if not Preferences.objectName():
            Preferences.setObjectName(u"Preferences")
        Preferences.resize(295, 99)
        self.verticalLayout = QVBoxLayout(Preferences)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.prefsOverButtonsLayout = QVBoxLayout()
        self.prefsOverButtonsLayout.setObjectName(u"prefsOverButtonsLayout")
        self.horizontalLayout_3 = QHBoxLayout()
        self.horizontalLayout_3.setObjectName(u"horizontalLayout_3")
        self.fontSizeLabel = QLabel(Preferences)
        self.fontSizeLabel.setObjectName(u"fontSizeLabel")

        self.horizontalLayout_3.addWidget(self.fontSizeLabel)

        self.fontSizeSpinBox = QSpinBox(Preferences)
        self.fontSizeSpinBox.setObjectName(u"fontSizeSpinBox")
        self.fontSizeSpinBox.setMinimum(6)
        self.fontSizeSpinBox.setValue(10)

        self.horizontalLayout_3.addWidget(self.fontSizeSpinBox)

        self.horizontalSpacer_2 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_3.addItem(self.horizontalSpacer_2)


        self.prefsOverButtonsLayout.addLayout(self.horizontalLayout_3)

        self.verticalSpacer = QSpacerItem(20, 40, QSizePolicy.Policy.Minimum, QSizePolicy.Policy.Expanding)

        self.prefsOverButtonsLayout.addItem(self.verticalSpacer)

        self.line = QFrame(Preferences)
        self.line.setObjectName(u"line")
        self.line.setFrameShape(QFrame.Shape.HLine)
        self.line.setFrameShadow(QFrame.Shadow.Sunken)

        self.prefsOverButtonsLayout.addWidget(self.line)

        self.horizontalLayout_2 = QHBoxLayout()
        self.horizontalLayout_2.setObjectName(u"horizontalLayout_2")
        self.horizontalSpacer = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_2.addItem(self.horizontalSpacer)

        self.buttonBox = QDialogButtonBox(Preferences)
        self.buttonBox.setObjectName(u"buttonBox")
        self.buttonBox.setStandardButtons(QDialogButtonBox.Apply|QDialogButtonBox.Cancel|QDialogButtonBox.Ok)

        self.horizontalLayout_2.addWidget(self.buttonBox)


        self.prefsOverButtonsLayout.addLayout(self.horizontalLayout_2)


        self.verticalLayout.addLayout(self.prefsOverButtonsLayout)


        self.retranslateUi(Preferences)

        QMetaObject.connectSlotsByName(Preferences)
    # setupUi

    def retranslateUi(self, Preferences):
        Preferences.setWindowTitle(QCoreApplication.translate("Preferences", u"Preferences", None))
        Preferences.setProperty(u"comment", QCoreApplication.translate("Preferences", u"\n"
"     Copyright 2020 Pixar                                                                   \n"
"                                                                                            \n"
"     Licensed under the terms set forth in the LICENSE.txt file available at\n"
"     https://openusd.org/license.\n"
"  ", None))
        self.fontSizeLabel.setText(QCoreApplication.translate("Preferences", u"Font Size", None))
    # retranslateUi

