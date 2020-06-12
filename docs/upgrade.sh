healthypi_loc="https://github.com/Protocentral/protocentral_healthypi_v4/releases/latest/download/healthypi-rpi.zip"

mkdir upgrade
curl -LO "$healthypi_loc" 
mv healthypi-rpi.zip upgrade/healthypi.zip

sudo rm -r --force *

rm -r upgrade/HealthyPi ||:
rm -r upgrade/application.linux-armv6hf ||:
unzip upgrade/healthypi.zip -d upgrade/
mv upgrade/application.linux-armv6hf/* .
rm -r upgrade/application.linux-armv6hf
