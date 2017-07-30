# should be run on cse machine, only used while finding issues with submission fails
echo 'Checking out original source tree...'
git clone ~cs9242/public_git/aos-2017

cd aos-2017

echo 'Checking out AOS2017 tag...'
git checkout -b dryrun AOS2017

echo "Applying diff $DIFF..."
git apply --index "../$DIFF"

echo "Reseting toolchain to arm-none-linux-gnueabi-..."
sed -i -e 's/^\(CONFIG_CROSS_COMPILER_PREFIX=\).*$/\1"arm-none-linux-gnueabi-"/' configs/aos_defconfig

echo "Making configuration..."
make aos_defconfig
make silentoldconfig

echo "Compiling..."
make app-images
