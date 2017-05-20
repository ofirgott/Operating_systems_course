mkdir 305645806
cd ./305645806
mkdir temp
cd ./temp
echo Ofir > Ofir
echo Gottesman > Gottesman
echo ofirg1 > ofirg1
cp Ofir ../Gottesman
cp Gottesman ../Ofir
rm Ofir
rm Gottesman
mv ofirg1 ..
cd ..
rmdir temp
echo "** Files in "30565806" directory: **"
ls -l -a
echo "** Content of "Ofir" file: **"
cat Ofir
echo "** Content of "Gottesman" file: **"
cat Gottesman
echo "** Content of "ofirg1" file: **"
cat ofirg1

