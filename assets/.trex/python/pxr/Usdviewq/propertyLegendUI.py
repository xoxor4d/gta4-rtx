# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'propertyLegendUI.ui'
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
from PySide6.QtWidgets import (QApplication, QFrame, QGraphicsView, QGridLayout,
    QHBoxLayout, QLabel, QSizePolicy, QSpacerItem,
    QVBoxLayout, QWidget)

class Ui_PropertyLegend(object):
    def setupUi(self, PropertyLegend):
        if not PropertyLegend.objectName():
            PropertyLegend.setObjectName(u"PropertyLegend")
        PropertyLegend.setWindowModality(Qt.NonModal)
        PropertyLegend.resize(654, 176)
        sizePolicy = QSizePolicy(QSizePolicy.Policy.Preferred, QSizePolicy.Policy.Preferred)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(PropertyLegend.sizePolicy().hasHeightForWidth())
        PropertyLegend.setSizePolicy(sizePolicy)
        self.gridLayout = QGridLayout(PropertyLegend)
        self.gridLayout.setObjectName(u"gridLayout")
        self.gridLayout_2 = QGridLayout()
        self.gridLayout_2.setSpacing(3)
        self.gridLayout_2.setObjectName(u"gridLayout_2")
        self.horizontalLayout = QHBoxLayout()
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.propertyLegendColorNoValue = QGraphicsView(PropertyLegend)
        self.propertyLegendColorNoValue.setObjectName(u"propertyLegendColorNoValue")
        self.propertyLegendColorNoValue.setMaximumSize(QSize(20, 15))

        self.horizontalLayout.addWidget(self.propertyLegendColorNoValue)

        self.propertyLegendLabelNoValue = QLabel(PropertyLegend)
        self.propertyLegendLabelNoValue.setObjectName(u"propertyLegendLabelNoValue")
        font = QFont()
        font.setBold(False)
        font.setItalic(False)
        self.propertyLegendLabelNoValue.setFont(font)

        self.horizontalLayout.addWidget(self.propertyLegendLabelNoValue)

        self.horizontalSpacer_8 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout.addItem(self.horizontalSpacer_8)


        self.gridLayout_2.addLayout(self.horizontalLayout, 0, 0, 1, 1)

        self.horizontalLayout_2 = QHBoxLayout()
        self.horizontalLayout_2.setObjectName(u"horizontalLayout_2")
        self.propertyLegendColorDefault = QGraphicsView(PropertyLegend)
        self.propertyLegendColorDefault.setObjectName(u"propertyLegendColorDefault")
        self.propertyLegendColorDefault.setMaximumSize(QSize(20, 15))

        self.horizontalLayout_2.addWidget(self.propertyLegendColorDefault)

        self.propertyLegendLabelDefault = QLabel(PropertyLegend)
        self.propertyLegendLabelDefault.setObjectName(u"propertyLegendLabelDefault")
        self.propertyLegendLabelDefault.setFont(font)

        self.horizontalLayout_2.addWidget(self.propertyLegendLabelDefault)

        self.horizontalSpacer_10 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_2.addItem(self.horizontalSpacer_10)


        self.gridLayout_2.addLayout(self.horizontalLayout_2, 0, 1, 1, 1)

        self.horizontalLayout_5 = QHBoxLayout()
        self.horizontalLayout_5.setObjectName(u"horizontalLayout_5")
        self.propertyLegendColorTimeSample = QGraphicsView(PropertyLegend)
        self.propertyLegendColorTimeSample.setObjectName(u"propertyLegendColorTimeSample")
        self.propertyLegendColorTimeSample.setMaximumSize(QSize(20, 15))

        self.horizontalLayout_5.addWidget(self.propertyLegendColorTimeSample)

        self.propertyLegendLabelTimeSample = QLabel(PropertyLegend)
        self.propertyLegendLabelTimeSample.setObjectName(u"propertyLegendLabelTimeSample")
        self.propertyLegendLabelTimeSample.setFont(font)

        self.horizontalLayout_5.addWidget(self.propertyLegendLabelTimeSample)

        self.horizontalSpacer_12 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_5.addItem(self.horizontalSpacer_12)


        self.gridLayout_2.addLayout(self.horizontalLayout_5, 0, 2, 1, 1)

        self.horizontalLayout_3 = QHBoxLayout()
        self.horizontalLayout_3.setObjectName(u"horizontalLayout_3")
        self.propertyLegendColorFallback = QGraphicsView(PropertyLegend)
        self.propertyLegendColorFallback.setObjectName(u"propertyLegendColorFallback")
        self.propertyLegendColorFallback.setMaximumSize(QSize(20, 15))

        self.horizontalLayout_3.addWidget(self.propertyLegendColorFallback)

        self.propertyLegendLabelFallback = QLabel(PropertyLegend)
        self.propertyLegendLabelFallback.setObjectName(u"propertyLegendLabelFallback")
        self.propertyLegendLabelFallback.setFont(font)

        self.horizontalLayout_3.addWidget(self.propertyLegendLabelFallback)

        self.horizontalSpacer_9 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_3.addItem(self.horizontalSpacer_9)


        self.gridLayout_2.addLayout(self.horizontalLayout_3, 1, 0, 1, 1)

        self.horizontalLayout_4 = QHBoxLayout()
        self.horizontalLayout_4.setObjectName(u"horizontalLayout_4")
        self.propertyLegendColorCustom = QGraphicsView(PropertyLegend)
        self.propertyLegendColorCustom.setObjectName(u"propertyLegendColorCustom")
        self.propertyLegendColorCustom.setMaximumSize(QSize(20, 15))

        self.horizontalLayout_4.addWidget(self.propertyLegendColorCustom)

        self.propertyLegendLabelCustom = QLabel(PropertyLegend)
        self.propertyLegendLabelCustom.setObjectName(u"propertyLegendLabelCustom")

        self.horizontalLayout_4.addWidget(self.propertyLegendLabelCustom)

        self.horizontalSpacer_11 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_4.addItem(self.horizontalSpacer_11)


        self.gridLayout_2.addLayout(self.horizontalLayout_4, 1, 1, 1, 1)

        self.horizontalLayout_6 = QHBoxLayout()
        self.horizontalLayout_6.setObjectName(u"horizontalLayout_6")
        self.propertyLegendColorValueClips = QGraphicsView(PropertyLegend)
        self.propertyLegendColorValueClips.setObjectName(u"propertyLegendColorValueClips")
        self.propertyLegendColorValueClips.setMaximumSize(QSize(20, 15))

        self.horizontalLayout_6.addWidget(self.propertyLegendColorValueClips)

        self.propertyLegendLabelValueClips = QLabel(PropertyLegend)
        self.propertyLegendLabelValueClips.setObjectName(u"propertyLegendLabelValueClips")
        self.propertyLegendLabelValueClips.setFont(font)

        self.horizontalLayout_6.addWidget(self.propertyLegendLabelValueClips)

        self.horizontalSpacer_13 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_6.addItem(self.horizontalSpacer_13)


        self.gridLayout_2.addLayout(self.horizontalLayout_6, 1, 2, 1, 1)

        self.horizontalLayout_15 = QHBoxLayout()
        self.horizontalLayout_15.setObjectName(u"horizontalLayout_15")
        self.propertyLegendColorSpline = QGraphicsView(PropertyLegend)
        self.propertyLegendColorSpline.setObjectName(u"propertyLegendColorSpline")
        self.propertyLegendColorSpline.setMaximumSize(QSize(20, 15))

        self.horizontalLayout_15.addWidget(self.propertyLegendColorSpline)

        self.propertyLegendLabelSpline = QLabel(PropertyLegend)
        self.propertyLegendLabelSpline.setObjectName(u"propertyLegendLabelSpline")
        self.propertyLegendLabelSpline.setFont(font)

        self.horizontalLayout_15.addWidget(self.propertyLegendLabelSpline)

        self.horizontalSpacer_16 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_15.addItem(self.horizontalSpacer_16)


        self.gridLayout_2.addLayout(self.horizontalLayout_15, 2, 2, 1, 1)


        self.gridLayout.addLayout(self.gridLayout_2, 0, 0, 1, 3)

        self.verticalLayout = QVBoxLayout()
        self.verticalLayout.setSpacing(3)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.horizontalLayout_9 = QHBoxLayout()
        self.horizontalLayout_9.setSpacing(3)
        self.horizontalLayout_9.setObjectName(u"horizontalLayout_9")
        self.propertyLegendAttrPlainIcon = QLabel(PropertyLegend)
        self.propertyLegendAttrPlainIcon.setObjectName(u"propertyLegendAttrPlainIcon")

        self.horizontalLayout_9.addWidget(self.propertyLegendAttrPlainIcon)

        self.propertyLegendAttrPlainDesc = QLabel(PropertyLegend)
        self.propertyLegendAttrPlainDesc.setObjectName(u"propertyLegendAttrPlainDesc")
        self.propertyLegendAttrPlainDesc.setFont(font)

        self.horizontalLayout_9.addWidget(self.propertyLegendAttrPlainDesc)

        self.horizontalSpacer = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_9.addItem(self.horizontalSpacer)


        self.verticalLayout.addLayout(self.horizontalLayout_9)

        self.horizontalLayout_10 = QHBoxLayout()
        self.horizontalLayout_10.setSpacing(3)
        self.horizontalLayout_10.setObjectName(u"horizontalLayout_10")
        self.propertyLegendRelPlainIcon = QLabel(PropertyLegend)
        self.propertyLegendRelPlainIcon.setObjectName(u"propertyLegendRelPlainIcon")

        self.horizontalLayout_10.addWidget(self.propertyLegendRelPlainIcon)

        self.propertyLegendRelPlainDesc = QLabel(PropertyLegend)
        self.propertyLegendRelPlainDesc.setObjectName(u"propertyLegendRelPlainDesc")
        self.propertyLegendRelPlainDesc.setFont(font)

        self.horizontalLayout_10.addWidget(self.propertyLegendRelPlainDesc)

        self.horizontalSpacer_2 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_10.addItem(self.horizontalSpacer_2)


        self.verticalLayout.addLayout(self.horizontalLayout_10)

        self.horizontalLayout_11 = QHBoxLayout()
        self.horizontalLayout_11.setSpacing(3)
        self.horizontalLayout_11.setObjectName(u"horizontalLayout_11")
        self.propertyLegendCompIcon = QLabel(PropertyLegend)
        self.propertyLegendCompIcon.setObjectName(u"propertyLegendCompIcon")

        self.horizontalLayout_11.addWidget(self.propertyLegendCompIcon)

        self.propertyLegendCompDesc = QLabel(PropertyLegend)
        self.propertyLegendCompDesc.setObjectName(u"propertyLegendCompDesc")
        self.propertyLegendCompDesc.setFont(font)

        self.horizontalLayout_11.addWidget(self.propertyLegendCompDesc)

        self.horizontalSpacer_3 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_11.addItem(self.horizontalSpacer_3)


        self.verticalLayout.addLayout(self.horizontalLayout_11)


        self.gridLayout.addLayout(self.verticalLayout, 1, 0, 1, 1)

        self.verticalLayout_2 = QVBoxLayout()
        self.verticalLayout_2.setObjectName(u"verticalLayout_2")
        self.horizontalLayout_12 = QHBoxLayout()
        self.horizontalLayout_12.setSpacing(3)
        self.horizontalLayout_12.setObjectName(u"horizontalLayout_12")
        self.propertyLegendConnIcon = QLabel(PropertyLegend)
        self.propertyLegendConnIcon.setObjectName(u"propertyLegendConnIcon")

        self.horizontalLayout_12.addWidget(self.propertyLegendConnIcon)

        self.propertyLegendConnDesc = QLabel(PropertyLegend)
        self.propertyLegendConnDesc.setObjectName(u"propertyLegendConnDesc")
        self.propertyLegendConnDesc.setFont(font)

        self.horizontalLayout_12.addWidget(self.propertyLegendConnDesc)

        self.horizontalSpacer_4 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_12.addItem(self.horizontalSpacer_4)


        self.verticalLayout_2.addLayout(self.horizontalLayout_12)

        self.horizontalLayout_13 = QHBoxLayout()
        self.horizontalLayout_13.setSpacing(3)
        self.horizontalLayout_13.setObjectName(u"horizontalLayout_13")
        self.propertyLegendTargetIcon = QLabel(PropertyLegend)
        self.propertyLegendTargetIcon.setObjectName(u"propertyLegendTargetIcon")

        self.horizontalLayout_13.addWidget(self.propertyLegendTargetIcon)

        self.propertyLegendTargetDesc = QLabel(PropertyLegend)
        self.propertyLegendTargetDesc.setObjectName(u"propertyLegendTargetDesc")
        self.propertyLegendTargetDesc.setMinimumSize(QSize(20, 20))
        self.propertyLegendTargetDesc.setFont(font)

        self.horizontalLayout_13.addWidget(self.propertyLegendTargetDesc)

        self.horizontalSpacer_5 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_13.addItem(self.horizontalSpacer_5)


        self.verticalLayout_2.addLayout(self.horizontalLayout_13)

        self.horizontalLayout_14 = QHBoxLayout()
        self.horizontalLayout_14.setObjectName(u"horizontalLayout_14")
        self.inheritedPropertyIcon = QLabel(PropertyLegend)
        self.inheritedPropertyIcon.setObjectName(u"inheritedPropertyIcon")
        self.inheritedPropertyIcon.setTextFormat(Qt.RichText)

        self.horizontalLayout_14.addWidget(self.inheritedPropertyIcon)

        self.inheritedPropertyText = QLabel(PropertyLegend)
        self.inheritedPropertyText.setObjectName(u"inheritedPropertyText")
        font1 = QFont()
        font1.setItalic(False)
        self.inheritedPropertyText.setFont(font1)
        self.inheritedPropertyText.setFrameShape(QFrame.NoFrame)
        self.inheritedPropertyText.setTextFormat(Qt.RichText)
        self.inheritedPropertyText.setAlignment(Qt.AlignCenter)
        self.inheritedPropertyText.setWordWrap(False)

        self.horizontalLayout_14.addWidget(self.inheritedPropertyText)

        self.horizontalSpacer_14 = QSpacerItem(13, 13, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_14.addItem(self.horizontalSpacer_14)


        self.verticalLayout_2.addLayout(self.horizontalLayout_14)


        self.gridLayout.addLayout(self.verticalLayout_2, 1, 1, 1, 1)

        self.verticalLayout_3 = QVBoxLayout()
        self.verticalLayout_3.setObjectName(u"verticalLayout_3")
        self.horizontalLayout_8 = QHBoxLayout()
        self.horizontalLayout_8.setSpacing(3)
        self.horizontalLayout_8.setObjectName(u"horizontalLayout_8")
        self.propertyLegendAttrWithConnIcon = QLabel(PropertyLegend)
        self.propertyLegendAttrWithConnIcon.setObjectName(u"propertyLegendAttrWithConnIcon")

        self.horizontalLayout_8.addWidget(self.propertyLegendAttrWithConnIcon)

        self.propertyLegendAttrWithConnDesc = QLabel(PropertyLegend)
        self.propertyLegendAttrWithConnDesc.setObjectName(u"propertyLegendAttrWithConnDesc")
        self.propertyLegendAttrWithConnDesc.setFont(font)

        self.horizontalLayout_8.addWidget(self.propertyLegendAttrWithConnDesc)

        self.horizontalSpacer_6 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_8.addItem(self.horizontalSpacer_6)


        self.verticalLayout_3.addLayout(self.horizontalLayout_8)

        self.horizontalLayout_7 = QHBoxLayout()
        self.horizontalLayout_7.setSpacing(3)
        self.horizontalLayout_7.setObjectName(u"horizontalLayout_7")
        self.propertyLegendRelWithTargetIcon = QLabel(PropertyLegend)
        self.propertyLegendRelWithTargetIcon.setObjectName(u"propertyLegendRelWithTargetIcon")

        self.horizontalLayout_7.addWidget(self.propertyLegendRelWithTargetIcon)

        self.propertyLegendRelWithTargetDesc = QLabel(PropertyLegend)
        self.propertyLegendRelWithTargetDesc.setObjectName(u"propertyLegendRelWithTargetDesc")
        self.propertyLegendRelWithTargetDesc.setFont(font)

        self.horizontalLayout_7.addWidget(self.propertyLegendRelWithTargetDesc)

        self.horizontalSpacer_7 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout_7.addItem(self.horizontalSpacer_7)


        self.verticalLayout_3.addLayout(self.horizontalLayout_7)

        self.horizontalSpacer_15 = QSpacerItem(40, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.verticalLayout_3.addItem(self.horizontalSpacer_15)


        self.gridLayout.addLayout(self.verticalLayout_3, 1, 2, 1, 1)


        self.retranslateUi(PropertyLegend)

        QMetaObject.connectSlotsByName(PropertyLegend)
    # setupUi

    def retranslateUi(self, PropertyLegend):
        PropertyLegend.setProperty(u"comment", QCoreApplication.translate("PropertyLegend", u"\n"
"     Copyright 2017 Pixar                                                                   \n"
"                                                                                            \n"
"     Licensed under the terms set forth in the LICENSE.txt file available at\n"
"     https://openusd.org/license.\n"
"  ", None))
        self.propertyLegendLabelNoValue.setText(QCoreApplication.translate("PropertyLegend", u"No Value", None))
        self.propertyLegendLabelDefault.setText(QCoreApplication.translate("PropertyLegend", u"Default", None))
        self.propertyLegendLabelTimeSample.setText(QCoreApplication.translate("PropertyLegend", u"Time Samples (Interpolated) ", None))
        self.propertyLegendLabelFallback.setText(QCoreApplication.translate("PropertyLegend", u"Fallback", None))
        self.propertyLegendLabelCustom.setText(QCoreApplication.translate("PropertyLegend", u"Custom", None))
        self.propertyLegendLabelValueClips.setText(QCoreApplication.translate("PropertyLegend", u"Value Clips (Interpolated)", None))
        self.propertyLegendLabelSpline.setText(QCoreApplication.translate("PropertyLegend", u"Spline (Interpolated)", None))
        self.propertyLegendAttrPlainDesc.setText(QCoreApplication.translate("PropertyLegend", u"Attribute", None))
        self.propertyLegendRelPlainDesc.setText(QCoreApplication.translate("PropertyLegend", u"Relationship", None))
        self.propertyLegendCompDesc.setText(QCoreApplication.translate("PropertyLegend", u"Computed Value", None))
        self.propertyLegendConnDesc.setText(QCoreApplication.translate("PropertyLegend", u"Connection", None))
        self.propertyLegendTargetDesc.setText(QCoreApplication.translate("PropertyLegend", u"Target", None))
        self.inheritedPropertyIcon.setText(QCoreApplication.translate("PropertyLegend", u"<small><i>(i)</i></small>", None))
        self.inheritedPropertyText.setText(QCoreApplication.translate("PropertyLegend", u"Inherited Property", None))
        self.propertyLegendAttrWithConnDesc.setText(QCoreApplication.translate("PropertyLegend", u"Attribute w/Connection(s)", None))
        self.propertyLegendRelWithTargetDesc.setText(QCoreApplication.translate("PropertyLegend", u"Relationship w/Target(s)", None))
    # retranslateUi

