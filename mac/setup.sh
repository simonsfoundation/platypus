export PATH=~/qt/5.3.1s/qtbase/bin/:$PATH

python ~/scripts/make_moc.py ../src
rcc -o tmp/resources.cpp ../src/platypus.qrc


