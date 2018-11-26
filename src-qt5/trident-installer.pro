QT       += core gui widgets concurrent


TARGET = trident-installer
target.path = /usr/local/bin

TEMPLATE = app

SOURCES += main.cpp \
		mainUI.cpp \
		backend.cpp

HEADERS  += mainUI.h \
		backend.h

FORMS    += mainUI.ui

RESOURCES = trident-installer.qrc

TRANSLATIONS =  i18n/tri-install_af.ts \
                i18n/tri-install_ar.ts \
                i18n/tri-install_az.ts \
                i18n/tri-install_bg.ts \
                i18n/tri-install_bn.ts \
                i18n/tri-install_bs.ts \
                i18n/tri-install_ca.ts \
                i18n/tri-install_cs.ts \
                i18n/tri-install_cy.ts \
                i18n/tri-install_da.ts \
                i18n/tri-install_de.ts \
                i18n/tri-install_el.ts \
                i18n/tri-install_en_GB.ts \
                i18n/tri-install_en_ZA.ts \
                i18n/tri-install_en_AU.ts \
                i18n/tri-install_es.ts \
                i18n/tri-install_et.ts \
                i18n/tri-install_eu.ts \
                i18n/tri-install_fa.ts \
                i18n/tri-install_fi.ts \
                i18n/tri-install_fr.ts \
                i18n/tri-install_fr_CA.ts \
                i18n/tri-install_gl.ts \
                i18n/tri-install_he.ts \
                i18n/tri-install_hi.ts \
                i18n/tri-install_hr.ts \
                i18n/tri-install_hu.ts \
                i18n/tri-install_id.ts \
                i18n/tri-install_is.ts \
                i18n/tri-install_it.ts \
                i18n/tri-install_ja.ts \
                i18n/tri-install_ka.ts \
                i18n/tri-install_ko.ts \
                i18n/tri-install_lt.ts \
                i18n/tri-install_lv.ts \
                i18n/tri-install_mk.ts \
                i18n/tri-install_mn.ts \
                i18n/tri-install_ms.ts \
                i18n/tri-install_mt.ts \
                i18n/tri-install_nb.ts \
                i18n/tri-install_nl.ts \
                i18n/tri-install_pa.ts \
                i18n/tri-install_pl.ts \
                i18n/tri-install_pt.ts \
                i18n/tri-install_pt_BR.ts \
                i18n/tri-install_ro.ts \
                i18n/tri-install_ru.ts \
                i18n/tri-install_sk.ts \
                i18n/tri-install_sl.ts \
                i18n/tri-install_sr.ts \
                i18n/tri-install_sv.ts \
                i18n/tri-install_sw.ts \
                i18n/tri-install_ta.ts \
                i18n/tri-install_tg.ts \
                i18n/tri-install_th.ts \
                i18n/tri-install_tr.ts \
                i18n/tri-install_uk.ts \
                i18n/tri-install_uz.ts \
                i18n/tri-install_vi.ts \
                i18n/tri-install_zh_CN.ts \
                i18n/tri-install_zh_HK.ts \
                i18n/tri-install_zh_TW.ts \
                i18n/tri-install_zu.ts

dotrans.path=/usr/local/share/trident-installer/i18n/
dotrans.extra=cd $$PWD/i18n && lrelease -nounfinished *.ts && cp *.qm $(INSTALL_ROOT)/usr/local/share/trident-installer/i18n/

#Some conf to redirect intermediate stuff in separate dirs
UI_DIR=./.build/ui/
MOC_DIR=./.build/moc/
OBJECTS_DIR=./.build/obj
RCC_DIR=./.build/rcc
QMAKE_DISTCLEAN += -r ./.build

#Setup the default place for installing icons (use scalable dir for variable-size icons)
icons.path = $${L_SHAREDIR}/icons/hicolor/scalable/apps

script.path = /usr/local/bin
script.files = scripts/start-trident-installer \
		scripts/activateNewBE \
		scripts/install-refind \
		scripts/detect-best-driver.sh \
		scripts/generate-xorg-conf.sh

xorg.path = /usr/local/share/trident-installer
xorg.files = files/xorg.conf.template

machid.path = /etc
machid.files = files/machine-id

curtheme.path = /usr/local/share/icons/default
curtheme.files = files/index.theme

loadcfg.path = /boot
loadcfg.files = files/loader.conf.local

INSTALLS += target dotrans script xorg machid curtheme loadcfg
