#!/bin/sh

rm -rf entomologist.app
make clean
make
cp -r translations entomologist.app/Contents/translations
cp entomologist_*.qm entomologist.app/Contents/translations
cp entomologist.translations.db entomologist.app/Contents/translations
macdeployqt entomologist.app -dmg
