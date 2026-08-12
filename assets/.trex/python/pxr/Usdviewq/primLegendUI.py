# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'primLegendUI.ui'
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
from PySide6.QtWidgets import (QApplication, QGraphicsView, QGridLayout, QLabel,
    QSizePolicy, QVBoxLayout, QWidget)

class Ui_PrimLegend(object):
    def setupUi(self, PrimLegend):
        if not PrimLegend.objectName():
            PrimLegend.setObjectName(u"PrimLegend")
        PrimLegend.resize(438, 131)
        sizePolicy = QSizePolicy(QSizePolicy.Policy.Preferred, QSizePolicy.Policy.Preferred)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(PrimLegend.sizePolicy().hasHeightForWidth())
        PrimLegend.setSizePolicy(sizePolicy)
        self.primLegendLayoutContainer = QVBoxLayout(PrimLegend)
        self.primLegendLayoutContainer.setObjectName(u"primLegendLayoutContainer")
        self.primLegendLayout = QGridLayout()
        self.primLegendLayout.setObjectName(u"primLegendLayout")
        self.primLegendColorHasArcs = QGraphicsView(PrimLegend)
        self.primLegendColorHasArcs.setObjectName(u"primLegendColorHasArcs")
        self.primLegendColorHasArcs.setMaximumSize(QSize(20, 15))

        self.primLegendLayout.addWidget(self.primLegendColorHasArcs, 0, 0, 1, 1)

        self.primLegendLabelHasArcs = QLabel(PrimLegend)
        self.primLegendLabelHasArcs.setObjectName(u"primLegendLabelHasArcs")
        font = QFont()
        font.setBold(False)
        font.setItalic(False)
        self.primLegendLabelHasArcs.setFont(font)

        self.primLegendLayout.addWidget(self.primLegendLabelHasArcs, 0, 1, 1, 1)

        self.primLegendColorInstance = QGraphicsView(PrimLegend)
        self.primLegendColorInstance.setObjectName(u"primLegendColorInstance")
        self.primLegendColorInstance.setMaximumSize(QSize(20, 15))

        self.primLegendLayout.addWidget(self.primLegendColorInstance, 0, 2, 1, 1)

        self.primLegendLabelInstance = QLabel(PrimLegend)
        self.primLegendLabelInstance.setObjectName(u"primLegendLabelInstance")
        self.primLegendLabelInstance.setFont(font)

        self.primLegendLayout.addWidget(self.primLegendLabelInstance, 0, 3, 1, 1)

        self.primLegendColorPrototype = QGraphicsView(PrimLegend)
        self.primLegendColorPrototype.setObjectName(u"primLegendColorPrototype")
        self.primLegendColorPrototype.setMaximumSize(QSize(20, 15))

        self.primLegendLayout.addWidget(self.primLegendColorPrototype, 0, 4, 1, 1)

        self.primLegendLabelPrototype = QLabel(PrimLegend)
        self.primLegendLabelPrototype.setObjectName(u"primLegendLabelPrototype")
        self.primLegendLabelPrototype.setFont(font)

        self.primLegendLayout.addWidget(self.primLegendLabelPrototype, 0, 5, 1, 1)

        self.primLegendColorNormal = QGraphicsView(PrimLegend)
        self.primLegendColorNormal.setObjectName(u"primLegendColorNormal")
        self.primLegendColorNormal.setMaximumSize(QSize(20, 15))

        self.primLegendLayout.addWidget(self.primLegendColorNormal, 0, 6, 1, 1)

        self.primLegendLabelNormal = QLabel(PrimLegend)
        self.primLegendLabelNormal.setObjectName(u"primLegendLabelNormal")
        self.primLegendLabelNormal.setFont(font)

        self.primLegendLayout.addWidget(self.primLegendLabelNormal, 0, 7, 1, 1)


        self.primLegendLayoutContainer.addLayout(self.primLegendLayout)

        self.primLegendLabelContainer = QVBoxLayout()
        self.primLegendLabelContainer.setObjectName(u"primLegendLabelContainer")
        self.primLegendLabelDimmed = QLabel(PrimLegend)
        self.primLegendLabelDimmed.setObjectName(u"primLegendLabelDimmed")

        self.primLegendLabelContainer.addWidget(self.primLegendLabelDimmed)

        self.primLegendLabelFontsAbstract = QLabel(PrimLegend)
        self.primLegendLabelFontsAbstract.setObjectName(u"primLegendLabelFontsAbstract")

        self.primLegendLabelContainer.addWidget(self.primLegendLabelFontsAbstract)

        self.primLegendLabelFontsUndefined = QLabel(PrimLegend)
        self.primLegendLabelFontsUndefined.setObjectName(u"primLegendLabelFontsUndefined")

        self.primLegendLabelContainer.addWidget(self.primLegendLabelFontsUndefined)

        self.primLegendLabelFontsDefined = QLabel(PrimLegend)
        self.primLegendLabelFontsDefined.setObjectName(u"primLegendLabelFontsDefined")

        self.primLegendLabelContainer.addWidget(self.primLegendLabelFontsDefined)


        self.primLegendLayoutContainer.addLayout(self.primLegendLabelContainer)


        self.retranslateUi(PrimLegend)

        QMetaObject.connectSlotsByName(PrimLegend)
    # setupUi

    def retranslateUi(self, PrimLegend):
        PrimLegend.setProperty(u"comment", QCoreApplication.translate("PrimLegend", u"\n"
"     Copyright 2017 Pixar                                                                   \n"
"                                                                                            \n"
"     Licensed under the terms set forth in the LICENSE.txt file available at\n"
"     https://openusd.org/license.\n"
"  ", None))
        self.primLegendLabelHasArcs.setText(QCoreApplication.translate("PrimLegend", u"HasArcs", None))
        self.primLegendLabelInstance.setText(QCoreApplication.translate("PrimLegend", u"Instance", None))
        self.primLegendLabelPrototype.setText(QCoreApplication.translate("PrimLegend", u"Prototype", None))
        self.primLegendLabelNormal.setText(QCoreApplication.translate("PrimLegend", u"Normal", None))
        self.primLegendLabelDimmed.setText(QCoreApplication.translate("PrimLegend", u"Dimmed colors denote inactive prims", None))
        self.primLegendLabelFontsAbstract.setText(QCoreApplication.translate("PrimLegend", u"Normal font indicates abstract prims(class and children)", None))
        self.primLegendLabelFontsUndefined.setText(QCoreApplication.translate("PrimLegend", u"Italic font indicates undefined prims(declared with over)", None))
        self.primLegendLabelFontsDefined.setText(QCoreApplication.translate("PrimLegend", u"Bold font indicates defined prims(declared with def)", None))
    # retranslateUi

