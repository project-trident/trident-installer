# $FreeBSD$

PORTNAME=	trident-installer
PORTVERSION=	%%CHGVERSION%%
CATEGORIES=	trident

MAINTAINER=	ken@ixsystems.com
COMMENT=	Graphical ISO installer for Project Trident

LICENSE=	BSD2CLAUSE

RUN_DEPENDS=	lumina-coreutils>=0:x11/lumina-coreutils \
		xorg-minimal>=0:x11/xorg-minimal \
		/boot/modules/drm.ko:graphics/drm-next-kmod \
		trident-icons>=0:trident/trident-icons \
		cursor-chameleon-pearl>=0:x11-themes/cursor-chameleon-pearl \
		xinit>=0:x11/xinit

USES=	qmake qt:5
USE_QT=	core gui widgets svg concurrent \
		buildtools_build linguist_build
USE_GITHUB=	yes
GH_ACCOUNT=	project-trident
GH_PROJECT=	trident-installer
GH_TAGNAME=	%%GHTAG%%
WRKSRC_SUBDIR=	src-qt5

.include <bsd.port.mk>
