#!/bin/sh

make TARGET=ARM clean
rm *~
rm */*~
rm */*/*~

echo 'cleaned to be committed'
