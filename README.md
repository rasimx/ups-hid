# ups-hid

чтобы плата адекватно определялась, нужно отредактировать 

~/.platformio/platforms/atmelavr/boards/leonardo.json

пример есть в текущем репо


## работа с Serial
гляди в platformio.ini

build_flags = 
    -DCDC_DISABLED ;  при включении этого флага перестает работать Serial и скомпилировать с ним не получится

Serial можно использовать для отладки на ноутбуке, но NUT с ним не дружит, поэтому перед заливкой, нужно удалить Serial