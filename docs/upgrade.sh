healthypi_loc="https://github.com/Protocentral/protocentral_healthypi_v4/releases/latest/download/healthypi-rpi.zip"

mkdir upgrade
wget "$healthypi_loc" -O "upgrade/healthypi.zip"

rm -r upgrade/HealthyPi ||:
rm -r upgrade/application.linux-armv6hf ||:
unzip upgrade/healthypi.zip -d upgrade/
mv upgrade/application.linux-armv6hf/* .
rm -r upgrade/application.linux-armv6hf