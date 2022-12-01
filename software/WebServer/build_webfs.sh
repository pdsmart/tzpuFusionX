#!/bin/bash

SRCDIR=`pwd`/webserver
WEBFSDIR=`pwd`/webfs
echo "Building into:$WEBFSDIR from $SRCDIR..."

mkdir -p ${WEBFSDIR}/css
mkdir -p ${WEBFSDIR}/js
mkdir -p ${WEBFSDIR}/font-awesome
mkdir -p ${WEBFSDIR}/font-awesome/css
mkdir -p ${WEBFSDIR}/font-awesome/fonts
mkdir -p ${WEBFSDIR}/images

(cd ${SRCDIR}/;
cp      favicon.ico                  ${WEBFSDIR}/
cp      version.txt                  ${WEBFSDIR}/
cp      index.html                   ${WEBFSDIR}/
cp      keymap.html                  ${WEBFSDIR}/keymap.html
cp      mouse.html                   ${WEBFSDIR}/mouse.html
cp      ota.html                     ${WEBFSDIR}/ota.html
cp      wifimanager.html             ${WEBFSDIR}/wifimanager.html


(cd ${SRCDIR}/css;
cp      bootstrap.min.css.gz         ${WEBFSDIR}/css/
gzip -c jquery.edittable.min.css   > ${WEBFSDIR}/css/jquery.edittable.min.css.gz
gzip -c sb-admin.css               > ${WEBFSDIR}/css/sb-admin.css.gz
gzip -c sharpkey.css               > ${WEBFSDIR}/css/sharpkey.css.gz
gzip -c style.css                  > ${WEBFSDIR}/css/style.css.gz
gzip -c styles.css                 > ${WEBFSDIR}/css/styles.css.gz
)

(cd ${SRCDIR}/font-awesome
)

(cd ${SRCDIR}/font-awesome/css
#cp      font-awesome.min.css.gz      ${WEBFSDIR}/font-awesome/css/
gzip -c font-awesome.css           > ${WEBFSDIR}/font-awesome/css/font-awesome.min.css.gz
)

(cd ${SRCDIR}/font-awesome/fonts
gzip -c fontawesome-webfont.woff   > ${WEBFSDIR}/font-awesome/fonts/fontawesome-webfont.woff.gz
#cp      fontawesome-webfont.ttf.gz   ${WEBFSDIR}/font-awesome/fonts/
#cp      fontawesome-webfont.woff.gz  ${WEBFSDIR}/font-awesome/fonts/
)

(cd ${SRCDIR}/images;
)

(cd ${SRCDIR}/js;
cp      140medley.min.js             ${WEBFSDIR}/js/
cp      bootstrap.min.js.gz          ${WEBFSDIR}/js/
gzip -c index.js                   > ${WEBFSDIR}/js/index.js.gz
gzip -c jquery.edittable.js        > ${WEBFSDIR}/js/jquery.edittable.js.gz
gzip -c jquery.edittable.min.js    > ${WEBFSDIR}/js/jquery.edittable.min.j.gz
cp      jquery.min.js.gz             ${WEBFSDIR}/js/
gzip -c keymap.js                  > ${WEBFSDIR}/js/keymap.js.gz
gzip -c mouse.js                   > ${WEBFSDIR}/js/mouse.js.gz
gzip -c ota.js                     > ${WEBFSDIR}/js/ota.js.gz
gzip -c wifimanager.js             > ${WEBFSDIR}/js/wifimanager.js.gz
)

)
