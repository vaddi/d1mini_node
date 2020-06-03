#!/usr/bin/env bash
#
## Add as function to .bashrc or .bash_profile (replace PATH by you real path!)
#
#espCli() {
#  ~/PATH/espGet.sh $1
#}

# Change Adresses to your needs
IPADDR=(
  esp01.speedport.ip
  esp02.speedport.ip
  esp05.speedport.ip
  esp07.speedport.ip
  esp10.speedport.ip
  esp11.speedport.ip
  esp12.speedport.ip
  esp13.speedport.ip
  esp14.speedport.ip
  esp15.speedport.ip
  esp16.speedport.ip
  esp17.speedport.ip
)

APIKEY="?apikey=i3xAEuhIVQiBjpes"
DNSSERVER="192.168.1.11"


#
# functions
#

pingcheck() {
  for DEV in "${IPADDR[@]}"
  do
    echo -n "${DEV} ping: "
    ping -t 1 -c 1 -q ${DEV} 2>&1 > /dev/null
    if [ $? -eq 0 ]; then
      echo "ok"
    else
      echo "failed"
    fi  
  done
}

curlreq() {
  if [ -z "$1" ]; then
    for DEV in "${IPADDR[@]}"
    do
      echo -n "${DEV} curl request: "
      curl --silent --connect-timeout 10 http://${DEV} 2>&1 > /dev/null
      if [ $? -eq 0 ]; then
        echo "ok"
      else
        echo "failed"
      fi
    done
  else
#    echo -n "${1} curl request: "
    return curl --silent --connect-timeout 10 http://${1} 2>&1 > /dev/null
#    if [ $? -eq 0 ]; then
#      echo "ok"
#    else
#      echo "failed"
#    fi
  fi
}

getIps() {
  for DEV in "${IPADDR[@]}"
  do
    echo -n "${DEV} request IP: "
    IP=$( dig a ${DEV} +short @${DNSSERVER} ) 
    if [ $? -eq 0 ] && [ ! -z "$IP" ]; then
      echo "${IP}"
    else
      echo "failed"
    fi
  done
}

restartall() {
  for DEV in "${IPADDR[@]}"
  do
    echo -n "${DEV} curl request /restart: "
    curlreq "${DEV}/restart${APIKEY}"
#    curl --silent --connect-timeout 10 http://${DEV}/restart${APIKEY} 2>&1 > /dev/null
    if [ $? -eq 0 ]; then
      echo "ok"
    else
      echo "failed"
    fi
  done
}

getVersions() {
  for DEV in "${IPADDR[@]}"
  do
    echo -n "${DEV} request Version: "
    #VERSION=curlreq "http://${DEV}/metrics${APIKEY}"
    #VERSION=$( echo ${VERSION} | grep 'version=' | cut -f2 -d\" )
    VERSION=$(curl --silent --connect-timeout 1 http://${DEV}/metrics${APIKEY} 2>&1 | grep 'version=' | cut -f2 -d\")
    if [ $? -eq 0 ] && [ ! -z "$VERSION" ]; then
      echo "${VERSION}"
    else
      echo "failed"
    fi
  done
}


getPlaces() {
  for DEV in "${IPADDR[@]}"
  do
    echo -n "${DEV} request Version: "
    PLACE=$(curl --silent --connect-timeout 1 http://${DEV}/metrics${APIKEY} 2>&1 | grep 'nodeplace=' | cut -f8 -d\")
    if [ $? -eq 0 ] && [ ! -z "$PLACE" ]; then
      echo "${PLACE}"
    else
      echo "failed"
    fi
  done
}


#
# optargs parsing
#

usage() { echo "$0 usage:" && grep " .)\ #" $0; exit 0; }

[ $# -eq 0 ] && usage

while getopts ":cipPrvh" arg; do
  case $arg in
    c) # curl request to all device
      curlreq
      ;;
    i) # get all IP Adress from local DNS
      getIps
      ;;
    p) # ping all devices
      pingcheck
      ;;
    P) # get Place from metrics
      getPlaces
      ;;
    r) # restart all devices
      restartall
      ;;
    v) # get Versions from metrics
      getVersions
      ;;
    h | *) # Display help.
      usage
      exit 0
      ;;
  esac
done

exit 0

# end of file
