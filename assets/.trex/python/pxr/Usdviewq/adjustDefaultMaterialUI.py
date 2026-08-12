# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'adjustDefaultMaterialUI.ui'
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
from PySide6.QtWidgets import (QApplication, QDialog, QDoubleSpinBox, QHBoxLayout,
    QLabel, QPushButton, QSizePolicy, QSpacerItem,
    QVBoxLayout, QWidget)

class Ui_AdjustDefaultMaterial(object):
    def setupUi(self, AdjustDefaultMaterial):
        if not AdjustDefaultMaterial.objectName():
            AdjustDefaultMaterial.setObjectName(u"AdjustDefaultMaterial")
        AdjustDefaultMaterial.resize(280, 123)
        sizePolicy = QSizePolicy(QSizePolicy.Policy.Minimum, QSizePolicy.Policy.Minimum)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(AdjustDefaultMaterial.sizePolicy().hasHeightForWidth())
        AdjustDefaultMaterial.setSizePolicy(sizePolicy)
        self.verticalLayout = QVBoxLayout(AdjustDefaultMaterial)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.verticalLayout_4 = QVBoxLayout()
        self.verticalLayout_4.setObjectName(u"verticalLayout_4")
        self.horizontalLayout = QHBoxLayout()
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.horizontalSpacer_4 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout.addItem(self.horizontalSpacer_4)

        self.ambientInt = QLabel(AdjustDefaultMaterial)
        self.ambientInt.setObjectName(u"ambientInt")
        self.ambientInt.setFocusPolicy(Qt.NoFocus)

        self.horizontalLayout.addWidget(self.ambientInt)

        self.ambientIntSpinBox = QDoubleSpinBox(AdjustDefaultMaterial)
        self.ambientIntSpinBox.setObjectName(u"ambientIntSpinBox")
        self.ambientIntSpinBox.setDecimals(1)
        self.ambientIntSpinBox.setMaximum(1.000000000000000)
        self.ambientIntSpinBox.setSingleStep(0.100000000000000)

        self.horizontalLayout.addWidget(self.ambientIntSpinBox)

        self.horizontalSpacer_5 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout.addItem(self.horizontalSpacer_5)


        self.verticalLayout_4.addLayout(self.horizontalLayout)

        self.horizontalLayout_2 = QHBoxLayout()
        self.horizontalLayout_2.setObjectName(u"horizontalLayout_2")
        self.horizontalSpacer_6 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_2.addItem(self.horizontalSpacer_6)

        self.specularInt = QLabel(AdjustDefaultMaterial)
        self.specularInt.setObjectName(u"specularInt")
        self.specularInt.setFocusPolicy(Qt.NoFocus)

        self.horizontalLayout_2.addWidget(self.specularInt)

        self.specularIntSpinBox = QDoubleSpinBox(AdjustDefaultMaterial)
        self.specularIntSpinBox.setObjectName(u"specularIntSpinBox")
        self.specularIntSpinBox.setDecimals(1)
        self.specularIntSpinBox.setMaximum(1.000000000000000)
        self.specularIntSpinBox.setSingleStep(0.100000000000000)

        self.horizontalLayout_2.addWidget(self.specularIntSpinBox)

        self.horizontalSpacer_7 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_2.addItem(self.horizontalSpacer_7)


        self.verticalLayout_4.addLayout(self.horizontalLayout_2)

        self.horizontalLayout_3 = QHBoxLayout()
        self.horizontalLayout_3.setObjectName(u"horizontalLayout_3")
        self.resetButton = QPushButton(AdjustDefaultMaterial)
        self.resetButton.setObjectName(u"resetButton")
        self.resetButton.setAutoDefault(False)

        self.horizontalLayout_3.addWidget(self.resetButton)

        self.horizontalSpacer_2 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_3.addItem(self.horizontalSpacer_2)

        self.doneButton = QPushButton(AdjustDefaultMaterial)
        self.doneButton.setObjectName(u"doneButton")

        self.horizontalLayout_3.addWidget(self.doneButton)


        self.verticalLayout_4.addLayout(self.horizontalLayout_3)


        self.verticalLayout.addLayout(self.verticalLayout_4)


        self.retranslateUi(AdjustDefaultMaterial)

        QMetaObject.connectSlotsByName(AdjustDefaultMaterial)
    # setupUi

    def retranslateUi(self, AdjustDefaultMaterial):
        AdjustDefaultMaterial.setWindowTitle(QCoreApplication.translate("AdjustDefaultMaterial", u"Adjust Default Material", None))
        AdjustDefaultMaterial.setProperty(u"comment", QCoreApplication.translate("AdjustDefaultMaterial", u"\n"
"     Copyright 2017 Pixar                                                                   \n"
"                                                                                            \n"
"     Licensed under the terms set forth in the LICENSE.txt file available at\n"
"     https://openusd.org/license.\n"
"  ", None))
        self.ambientInt.setText(QCoreApplication.translate("AdjustDefaultMaterial", u"Ambient Intensity", None))
        self.specularInt.setText(QCoreApplication.translate("AdjustDefaultMaterial", u"Specular Intensity", None))
        self.resetButton.setText(QCoreApplication.translate("AdjustDefaultMaterial", u"Reset", None))
        self.doneButton.setText(QCoreApplication.translate("AdjustDefaultMaterial", u"Done", None))
    # retranslateUi

