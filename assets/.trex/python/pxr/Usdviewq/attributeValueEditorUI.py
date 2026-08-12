# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'attributeValueEditorUI.ui'
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
from PySide6.QtWidgets import (QApplication, QSizePolicy, QStackedWidget, QTextBrowser,
    QVBoxLayout, QWidget)

class Ui_AttributeValueEditor(object):
    def setupUi(self, AttributeValueEditor):
        if not AttributeValueEditor.objectName():
            AttributeValueEditor.setObjectName(u"AttributeValueEditor")
        AttributeValueEditor.resize(400, 300)
        self.verticalLayout = QVBoxLayout(AttributeValueEditor)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.verticalLayout.setContentsMargins(0, 0, 0, 0)
        self.stackedWidget = QStackedWidget(AttributeValueEditor)
        self.stackedWidget.setObjectName(u"stackedWidget")
        self.stackedWidget.setLineWidth(0)
        self.valueViewer = QTextBrowser()
        self.valueViewer.setObjectName(u"valueViewer")
        self.stackedWidget.addWidget(self.valueViewer)

        self.verticalLayout.addWidget(self.stackedWidget)


        self.retranslateUi(AttributeValueEditor)

        self.stackedWidget.setCurrentIndex(0)


        QMetaObject.connectSlotsByName(AttributeValueEditor)
    # setupUi

    def retranslateUi(self, AttributeValueEditor):
        AttributeValueEditor.setWindowTitle(QCoreApplication.translate("AttributeValueEditor", u"Form", None))
        AttributeValueEditor.setProperty(u"comment", QCoreApplication.translate("AttributeValueEditor", u"\n"
"     Copyright 2016 Pixar                                                                   \n"
"                                                                                            \n"
"     Licensed under the terms set forth in the LICENSE.txt file available at\n"
"     https://openusd.org/license.\n"
"  ", None))
    # retranslateUi

