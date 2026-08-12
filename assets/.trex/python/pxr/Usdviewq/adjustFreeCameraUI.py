# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'adjustFreeCameraUI.ui'
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
from PySide6.QtWidgets import (QApplication, QCheckBox, QDialog, QDoubleSpinBox,
    QHBoxLayout, QLabel, QSizePolicy, QVBoxLayout,
    QWidget)

class Ui_AdjustFreeCamera(object):
    def setupUi(self, AdjustFreeCamera):
        if not AdjustFreeCamera.objectName():
            AdjustFreeCamera.setObjectName(u"AdjustFreeCamera")
        AdjustFreeCamera.resize(331, 140)
        self.verticalLayout = QVBoxLayout(AdjustFreeCamera)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.horizontalLayout = QHBoxLayout()
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.verticalLayout_2 = QVBoxLayout()
        self.verticalLayout_2.setObjectName(u"verticalLayout_2")
        self.overrideNear = QCheckBox(AdjustFreeCamera)
        self.overrideNear.setObjectName(u"overrideNear")
        self.overrideNear.setFocusPolicy(Qt.NoFocus)

        self.verticalLayout_2.addWidget(self.overrideNear)

        self.overrideFar = QCheckBox(AdjustFreeCamera)
        self.overrideFar.setObjectName(u"overrideFar")
        self.overrideFar.setFocusPolicy(Qt.NoFocus)

        self.verticalLayout_2.addWidget(self.overrideFar)


        self.horizontalLayout.addLayout(self.verticalLayout_2)

        self.verticalLayout_3 = QVBoxLayout()
        self.verticalLayout_3.setObjectName(u"verticalLayout_3")
        self.nearSpinBox = QDoubleSpinBox(AdjustFreeCamera)
        self.nearSpinBox.setObjectName(u"nearSpinBox")
        sizePolicy = QSizePolicy(QSizePolicy.Policy.Preferred, QSizePolicy.Policy.Preferred)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.nearSpinBox.sizePolicy().hasHeightForWidth())
        self.nearSpinBox.setSizePolicy(sizePolicy)
        self.nearSpinBox.setDecimals(3)
        self.nearSpinBox.setMinimum(0.000000000000000)
        self.nearSpinBox.setMaximum(1000000000000.000000000000000)
        self.nearSpinBox.setSingleStep(1.000000000000000)

        self.verticalLayout_3.addWidget(self.nearSpinBox)

        self.farSpinBox = QDoubleSpinBox(AdjustFreeCamera)
        self.farSpinBox.setObjectName(u"farSpinBox")
        sizePolicy.setHeightForWidth(self.farSpinBox.sizePolicy().hasHeightForWidth())
        self.farSpinBox.setSizePolicy(sizePolicy)
        self.farSpinBox.setDecimals(3)
        self.farSpinBox.setMinimum(0.000000000000000)
        self.farSpinBox.setMaximum(1000000000000.000000000000000)
        self.farSpinBox.setSingleStep(1.000000000000000)

        self.verticalLayout_3.addWidget(self.farSpinBox)


        self.horizontalLayout.addLayout(self.verticalLayout_3)


        self.verticalLayout.addLayout(self.horizontalLayout)

        self.horizontalLayout_2 = QHBoxLayout()
        self.horizontalLayout_2.setObjectName(u"horizontalLayout_2")
        self.lockFreeCamAspect = QCheckBox(AdjustFreeCamera)
        self.lockFreeCamAspect.setObjectName(u"lockFreeCamAspect")
        self.lockFreeCamAspect.setFocusPolicy(Qt.NoFocus)

        self.horizontalLayout_2.addWidget(self.lockFreeCamAspect)

        self.freeCamAspect = QDoubleSpinBox(AdjustFreeCamera)
        self.freeCamAspect.setObjectName(u"freeCamAspect")
        self.freeCamAspect.setDecimals(3)
        self.freeCamAspect.setMinimum(0.000000000000000)
        self.freeCamAspect.setSingleStep(0.100000000000000)

        self.horizontalLayout_2.addWidget(self.freeCamAspect)


        self.verticalLayout.addLayout(self.horizontalLayout_2)

        self.horizontalLayout_3 = QHBoxLayout()
        self.horizontalLayout_3.setObjectName(u"horizontalLayout_3")
        self.freeCamFovLabel = QLabel(AdjustFreeCamera)
        self.freeCamFovLabel.setObjectName(u"freeCamFovLabel")
        self.freeCamFovLabel.setFocusPolicy(Qt.NoFocus)

        self.horizontalLayout_3.addWidget(self.freeCamFovLabel)

        self.freeCamFov = QDoubleSpinBox(AdjustFreeCamera)
        self.freeCamFov.setObjectName(u"freeCamFov")
        self.freeCamFov.setDecimals(3)
        self.freeCamFov.setMinimum(0.000000000000000)
        self.freeCamFov.setMaximum(180.000000000000000)
        self.freeCamFov.setSingleStep(1.000000000000000)

        self.horizontalLayout_3.addWidget(self.freeCamFov)


        self.verticalLayout.addLayout(self.horizontalLayout_3)


        self.retranslateUi(AdjustFreeCamera)

        QMetaObject.connectSlotsByName(AdjustFreeCamera)
    # setupUi

    def retranslateUi(self, AdjustFreeCamera):
        AdjustFreeCamera.setWindowTitle(QCoreApplication.translate("AdjustFreeCamera", u"Adjust Free Camera", None))
        AdjustFreeCamera.setProperty(u"comment", QCoreApplication.translate("AdjustFreeCamera", u"\n"
"     Copyright 2016 Pixar                                                                   \n"
"                                                                                            \n"
"     Licensed under the terms set forth in the LICENSE.txt file available at\n"
"     https://openusd.org/license.\n"
"  ", None))
        self.overrideNear.setText(QCoreApplication.translate("AdjustFreeCamera", u"Override Near", None))
        self.overrideFar.setText(QCoreApplication.translate("AdjustFreeCamera", u"Override Far", None))
        self.lockFreeCamAspect.setText(QCoreApplication.translate("AdjustFreeCamera", u"Aspect Ratio", None))
        self.freeCamFovLabel.setText(QCoreApplication.translate("AdjustFreeCamera", u"FOV (degrees)", None))
    # retranslateUi

