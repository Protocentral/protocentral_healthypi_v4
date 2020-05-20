#!/bin/bash

: <<'DISCLAIMER'

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

This script is licensed under the terms of the MIT license.
Unless otherwise noted, code reproduced herein
was written for this script.

- Team ProtoCentral -
- modified from the original script from Pimoroni -
- The Pimoroni Crew -

DISCLAIMER

spacereq=50 # minimum size required on root partition in MB
forcesudo="yes" # whether the script requires to be ran with root privileges
promptreboot="no" # whether the script should always prompt user to reboot
uartreq="yes" # whether uart communication is required
armhfonly="yes" # whether the script is allowed to run on other arch
armv6="yes" # whether armv6 processors are supported
armv7="yes" # whether armv7 processors are supported
armv8="yes" # whether armv8 processors are supported
raspbianonly="no" # whether the script is allowed to run on other OSes
osreleases=( "Raspbian" "Kano" "Mate" "PiTop" "RetroPie" ) # list os-releases supported
oswarning=( "Debian" "Ubuntu" ) # list experimental os-releases
osdeny=( "Darwin" "Kali" "Linaro" ) # list os-releases specifically disallowed
debugmode="yes" # whether the script should use debug routines

gitreponame="healthypi" # the name of the git project repo
gitusername="protocentral" # the name of the git user to fetch repo from
gitrepobranch="master" # repo branch to checkout
gitrepotop="root" # the name of the dir to base repo from
gitrepoclone="yes" # whether the git repo is to be cloned locally
gitclonedir="source" # the name of the local dir for repo

BOOTCMD=/boot/cmdline.txt
CONFIG=/boot/config.txt
APTSRC=/etc/apt/sources.list
INITABCONF=/etc/inittab
DTBODIR=/boot/overlays

RASPOOL="http://mirrordirector.raspbian.org/raspbian/pool"
RPIPOOL="http://archive.raspberrypi.org/debian/pool"
DEBPOOL="http://ftp.debian.org/debian/pool"
GETPOOL="https://get.pimoroni.com"

inform() {
    echo -e "$(tput setaf 6)$1$(tput sgr0)"
}

confirm() {
    if [ "$1" == '-y' ]; then
        true
    else
        read -r -p "$1 [y/N] " response < /dev/tty
        if [[ $response =~ ^(yes|y|Y)$ ]]; then
            true
        else
            false
        fi
    fi
}

prompt() {
        read -r -p "$1 [y/N] " response < /dev/tty
        if [[ $response =~ ^(yes|y|Y)$ ]]; then
            true
        else
            false
        fi
}

success() {
    echo -e "$(tput setaf 2)$1$(tput sgr0)"
}

inform() {
    echo -e "$(tput setaf 6)$1$(tput sgr0)"
}

warning() {
    echo -e "$(tput setaf 1)$1$(tput sgr0)"
}

newline() {
    echo ""
}

progress() {
    count=0
    until [ $count -eq $1 ]; do
        echo -n "..." && sleep 1
        ((count++))
    done
    echo
}
sudocheck() {
    if [ $(id -u) -ne 0 ]; then
        echo -e "Install must be run as root. Try 'sudo ./$scriptname'\n"
        exit 1
    fi
}

sysclean() {
    sudo apt-get clean && sudo apt-get autoclean
    sudo apt-get -y autoremove &> /dev/null
}

sysupdate() {
    if ! $UPDATE_DB; then
        echo "Updating apt indexes..." && progress 3 &
        sudo apt-get update 1> /dev/null || { warning "Apt failed to update indexes!" && exit 1; }
        echo "Reading package lists..."
        progress 3 && UPDATE_DB=true
    fi
}

sysupgrade() {
    sudo apt-get upgrade
    sudo apt-get clean && sudo apt-get autoclean
    sudo apt-get -y autoremove &> /dev/null
}

sysreboot() {
    warning "Some changes made to your system require"
    warning "your computer to reboot to take effect."
    echo
    if prompt "Would you like to reboot now?"; then
        sync && sudo reboot
    fi
}

arch_check() {
    IS_ARMHF=false
    IS_ARMv6=false

    if uname -m | grep -q "armv.l"; then
        IS_ARMHF=true
        if uname -m | grep -q "armv6l"; then
            IS_ARMv6=true
        fi
    fi
}

os_check() {
    IS_MACOSX=false
    IS_RASPBIAN=false
    IS_SUPPORTED=false
    IS_EXPERIMENTAL=false
    OS_NAME="Unknown"

    if uname -s | grep -q "Darwin"; then
        OS_NAME="Darwin" && IS_MACOSX=true
    elif cat /etc/os-release | grep -q "Kali"; then
        OS_NAME="Kali"
    elif [ -d ~/.kano-settings ] || [ -d ~/.kanoprofile ]; then
        OS_NAME="Kano"
    elif whoami | grep -q "linaro"; then
        OS_NAME="Linaro"
    elif [ -d ~/.config/ubuntu-mate ];then
        OS_NAME="Mate"
    elif [ -d ~/.pt-os-dashboard ] || [ -d ~/.pt-dashboard ] || [ -f ~/.pt-dashboard-config ]; then
        OS_NAME="PiTop"
    elif command -v emulationstation > /dev/null; then
        OS_NAME="RetroPie"
    elif cat /etc/os-release | grep -q "Raspbian"; then
        OS_NAME="Raspbian" && IS_RASPBIAN=true
    elif cat /etc/os-release | grep -q "Debian"; then
        OS_NAME="Debian"
    elif cat /etc/os-release | grep -q "Ubuntu"; then
        OS_NAME="Ubuntu"
    fi

    if [[ " ${osreleases[@]} " =~ " ${OS_NAME} " ]]; then
        IS_SUPPORTED=true
    fi
    if [[ " ${oswarning[@]} " =~ " ${OS_NAME} " ]]; then
        IS_EXPERIMENTAL=true
    fi
}

raspbian_check() {
    IS_SUPPORTED=false
    IS_EXPERIMENTAL=false

    if [ -f /etc/os-release ]; then
        if cat /etc/os-release | grep -q "/sid"; then
            IS_SUPPORTED=false && IS_EXPERIMENTAL=true
        elif cat /etc/os-release | grep -q "stretch"; then
            IS_SUPPORTED=false && IS_EXPERIMENTAL=true
        elif cat /etc/os-release | grep -q "buster"; then
            IS_SUPPORTED=true && IS_EXPERIMENTAL=false
        elif cat /etc/os-release | grep -q "jessie"; then
            IS_SUPPORTED=true && IS_EXPERIMENTAL=false
        elif cat /etc/os-release | grep -q "wheezy"; then
            IS_SUPPORTED=true && IS_EXPERIMENTAL=false
        else
            IS_SUPPORTED=false && IS_EXPERIMENTAL=false
        fi
    fi
}

home_dir() {
    if [ $EUID -ne 0 ]; then
        if $IS_MACOSX; then
            USER_HOME=$(dscl . -read /Users/$USER NFSHomeDirectory | cut -d: -f2)
        else
            USER_HOME=$(getent passwd $USER | cut -d: -f6)
        fi
    else
        warning "Running as root, please log in as a regular user with sudo rights!"
        echo && exit 1
    fi
}

space_chk() {
    if command -v stat > /dev/null && ! $IS_MACOSX; then
        if [ $spacereq -gt $(($(stat -f -c "%a*%S" /)/10**6)) ];then
            echo
            warning  "There is not enough space left to proceed with  installation"
            if confirm "Would you like to attempt to expand your filesystem?"; then
                \curl -sS $GETPOOL/expandfs | sudo bash
                exit 1
            else
                echo && exit 1
            fi
        fi
    fi
}

timestamp() {
    date +%Y%m%d-%H%M
}

check_network() {
    sudo ping -q -w 1 -c 1 `ip r | grep default | cut -d ' ' -f 3 | head -n 1` &> /dev/null && return 0 || return 1
}


launch_url() {
    check_network || (error_box "You don't appear to be connected to the internet, please check your connection and try again!" && exit 1)
    if command -v xdg-open > /dev/null; then
        xdg-open "$1" && return 0
    else
      error_box "There was an error attempting to launch your browser!"
    fi
}

get_install() {
    check_network || (error_box "You don't appear to be connected to the internet, please check your connection and try again!" && exit 1)
    if [ "$1" != diagnostic ];then
        sysupdate && UPDATE_DB=true
    fi
    if ! command -v curl > /dev/null; then
        apt_pkg_install "curl"
    fi
    \curl -sS https://get.pimoroni.com/$1 | bash -s - "-y"
    read -p "Press Enter to continue..." < /dev/tty
}

apt_pkg_req() {
    APT_CHK=$(dpkg-query -W -f='${Status}\n' "$1" 2> /dev/null | grep "install ok installed")

    if [ "" == "$APT_CHK" ]; then
        echo "$1 is required"
        true
    else
        echo "$1 is already installed"
        false
    fi
}

apt_pkg_install() {
    echo "Installing $1..."
    sudo apt-get --yes install "$1" 1> /dev/null || { echo -e "Apt failed to install $1!\nFalling back on pypi..." && return 1; }
    echo
}

apt_deb_chk() {
    BEFORE=$(dpkg-query -W "$1" 2> /dev/null)
    sudo apt-get --yes install "$1" &> /dev/null || return 1
    AFTER=$(dpkg-query -W "$1" 2> /dev/null)
    if [ "$BEFORE" == "$AFTER" ]; then
        echo "$1 is already the newest version"
    else
        echo "$1 was successfully upgraded"
    fi
}

apt_deb_install() {
    echo "Installing $1..."
    if [[ "$1" != *".deb"* ]]; then
        sudo apt-get --yes install "$1" &> /dev/null || inform "Apt failed to install $1!\nFalling back on pypi..."
        dpkg-query -W -f='${Status}\n' "$1" 2> /dev/null | grep "install ok installed"
    else
        DEBDIR=`mktemp -d /tmp/pimoroni.XXXXXX`
        cd $DEBDIR
        wget "$GETPOOL/resources/$1" &> /dev/null
        sudo dpkg -i "$DEBDIR/$1" | grep "Installing $1"
    fi
    echo
}



usb_max_power() {
    if grep -q "^max_usb_current=1$" $CONFIG; then
        echo -e "\nMax USB current setting already active"
    else
        echo -e "\nAdjusting USB current setting in $CONFIG"
        echo "max_usb_current=1" | sudo tee -a $CONFIG &> /dev/null
    fi
}

add_dtoverlay() {
    if grep -q "^dtoverlay=$1" $CONFIG; then
        echo -e "\n$1 overlay already active"
    elif grep -q "^#dtoverlay=$1" $CONFIG; then
        sudo sed -i "s|^#dtoverlay=$1|dtoverlay=$1|" $CONFIG &> /dev/null
        echo -e "\nAdding $1 overlay to $CONFIG"
    else
        echo "dtoverlay=$1" | sudo tee -a $CONFIG &> /dev/null
        echo -e "\nAdding $1 overlay to $CONFIG"
    fi
}

remove_dtoverlay() {
    sudo sed -i "s|^dtoverlay=$1|#dtoverlay=$1|" $CONFIG &> /dev/null
}

get_init_sys() {
    if command -v systemctl > /dev/null && systemctl | grep -q '\-\.mount'; then
        SYSTEMD=1
    elif [ -f /etc/init.d/cron ] && [ ! -h /etc/init.d/cron ]; then
        SYSTEMD=0
    else
        echo "Unrecognised init system" && exit 1
    fi
}

get_hw_rev() {
    hwrid=$(grep "^Revision" /proc/cpuinfo | rev | cut -c 2-3 | rev)
    if [ "$hwrid" == "00" ] || [ "$hwrid" == "01" ];then # Pi 1
        hwver="$(grep "^Revision" /proc/cpuinfo | rev | cut -c 1-4 | rev)"
        if [ "$hwrid" == "00" ];then
            hwgen="126" # P1
        elif [ "$hwrid" == "01" ];then
            hwgen="140" # J8
        fi
    else
        hwver="$(grep "^Revision" /proc/cpuinfo | rev | cut -c 1-6 | rev)"
        if [ "$hwrid" == "04" ];then # Pi 2
            hwgen="240"
        elif [ "$hwrid" == "08" ];then # Pi 3
            hwgen="340"
        elif [ "$hwrid" == "09" ];then # Pi 0
            hwgen="040"
        else # Unknown
            hwgen="000"
        fi
    fi
    echo $hwgen
}


: <<'MAINSTART'

Perform all variables declarations as well as function definition
above this section for clarity, thanks!

MAINSTART

# intro message


newline
inform  "Welcome to the HealthyPI Installer Scipt"
inform  "----------------------------------------"
inform  "Please ensure that your have a good internet connection before proceeding"
newline

if [ "$1" != '-y' ]; then
	inform "This script will do the following actions:"
	inform "1. Disable the serial console on the Raspberry Pi"
	inform "2. Disable Bluetooth UART port to make the pins available"
	inform "3. Download and install the HealthyPi GUI Application"
	inform "4. Set the HealthyPi application to auto-start on boot of the Pi"
	newline
fi

arch_check
os_check
space_chk
#home_dir

if ! $IS_ARMHF; then
    warning "This hardware is not supported, sorry!"
    warning "Config files have been left untouched"
    newline && exit 1
fi

if $IS_ARMv8 && [ $armv8 == "no" ]; then
    warning "Sorry, your CPU is not supported by this installer"
    newline && exit 1
elif $IS_ARMv7 && [ $armv7 == "no" ]; then
    warning "Sorry, your CPU is not supported by this installer"
    newline && exit 1
elif $IS_ARMv6 && [ $armv6 == "no" ]; then
    warning "Sorry, your CPU is not supported by this installer"
    newline && exit 1
fi

if [ $raspbianonly == "yes" ] && ! $IS_RASPBIAN;then
        warning "This script is intended for Raspbian on a Raspberry Pi!"
        newline && exit 1
fi

if $IS_RASPBIAN; then
    raspbian_check
    if ! $IS_SUPPORTED && ! $IS_EXPERIMENTAL; then
        newline && warning "--- Warning ---" && newline
        echo "The $productname installer"
        echo "does not work on this version of Raspbian."
        echo "Check https://github.com/$gitusername/$gitreponame"
        echo "for additional information and support"
        newline && exit 1
    fi
fi

if ! $IS_SUPPORTED && ! $IS_EXPERIMENTAL; then
        warning "Your operating system is not supported, sorry!"
        newline && exit 1
fi

if $IS_EXPERIMENTAL; then
    warning "Support for your operating system is experimental. Please visit"
    warning "forums.pimoroni.com if you experience issues with this product."
    newline
fi

if [ $forcesudo == "yes" ]; then
    sudocheck
fi

if confirm "Do you wish to continue?"; then

# basic environment preparation

    newline && echo "Checking environment..."

    if ! command -v curl > /dev/null; then
        apt_pkg_install "curl"
    fi
    if ! command -v wget > /dev/null; then
        apt_pkg_install "wget"
    fi

# hardware setup

    echo "Checking hardware requirements..."
	newline && echo "The serial console must be disabled for HealthyPi to work"

	get_init_sys
	get_hw_rev

	if [ -f $DTBODIR/pi3-disable-bt.dtbo ] && [ $hwgen == "340" ]; then
		if ! grep -q "^dtoverlay=pi3-disable-bt$" $CONFIG && ! grep -q "^dtoverlay=pi3-miniuart-bt$" $CONFIG; then
			newline
			warning "Communication needs to run on UART0 for reliable operation!"
			newline
			echo "Disabling the on-board Bluetooth adapter can achieve this result."
			echo "We can do this for you now by loading a dedicated overlay."
			if confirm "Would you like the pi3-disable-bt device tree overlay to load on boot?"; then
				newline
				echo "dtoverlay=pi3-disable-bt" | sudo tee -a $CONFIG &> /dev/null
				warning "The on-board Bluetooth adapter has been disabled."
				newline
				echo "If you wish to re-enable it, you may do so by commenting out"
				echo "the line starting with 'dtoverlay=pi3-disable-bt' in $CONFIG"
			fi
		fi
	fi

	if [ $SYSTEMD -eq 0 ]; then
		sed -i "s|^.*:.*:respawn:.*ttyAMA0|#&|" $INITABCONF &> /dev/null
	fi
	sed -i -e "s|console=ttyAMA0,[0-9]\+ ||" -e "s|console=serial0,[0-9]\+ ||" $BOOTCMD &> /dev/null
	sed -i -e "/^enable_uart=0/d" -e "/^enable_uart=1/d" $CONFIG &> /dev/null

	success "Your Raspberry Pi is ready for UART communication"
	ASK_TO_REBOOT=true

# apt repo install

    echo "Checking install requirements..."

    echo "Checking for dependencies..." && newline

    if $REMOVE_PKG; then
        for pkgrm in ${pkgremove[@]}; do
            warning "Installed package conflicts with requirements"
            sudo apt-get remove "$pkgrm"
        done
    fi
    for pkgdep in ${coredeplist[@]}; do
        if apt_pkg_req "$pkgdep"; then
            apt_pkg_install "$pkgdep"
        fi
    done

    installdir="/opt/healthypi"

# git release install

	cd /tmp
	rm -rf application.linux-armv6hf
    rm -rf healthypi-rpi.zip
	rm -rf /opt/healthypi_gui

	curl -LO https://github.com/Protocentral/protocentral_healthypi_v4/releases/latest/download/healthypi-rpi.zip

	unzip healthypi-rpi.zip

	mv application.linux-armv6hf healthypi_gui
	cp -r healthypi_gui /opt/healthypi_gui
	rm -rf application.linux-armv6hf.zip healthypi_gui

# Auto-start HealthyPi on boot

	grep HealthyPI /etc/xdg/lxsession/LXDE-pi/autostart >/dev/null
	if [ $? -eq 1 ]; then
		# Insert eyes.py into rc.local before final 'exit 0'
    sed -i -e "\/opt\/healthypi_gui\/gui/d" /etc/xdg/lxsession/LXDE-pi/autostart
		echo '@/opt/healthypi_gui/gui &' >> /etc/xdg/lxsession/LXDE-pi/autostart
	#sed -i "$a \/opt\/healthypi_gui\/HealthyPI\&\\nexit 0/g" ~.config/lxsession/LXDE-pi/autostart >/dev/null
	fi

# Finish-up

    if $IS_EXPERIMENTAL; then
        warning "Support for your operating system is experimental. Please visit"
        warning "protocentral.com/healthypi if you experience issues with this product."
        newline
    fi

    if [ "$FORCE" != '-y' ]; then
        if [ $promptreboot == "yes" ] || $ASK_TO_REBOOT; then
            sysreboot && newline
        fi
    fi
else
    newline && echo "Aborting..." && newline
fi

exit 0
