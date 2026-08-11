sudo apt update
sudo apt install avahi-daemon avahi-utils -y
sudo systemctl enable --now avahi-daemon
