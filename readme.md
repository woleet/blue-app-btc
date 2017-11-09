# woleet.id-nanos

Mainly based on https://github.com/LedgerHQ/blue-app-btc

To create the virtualenv, needed to install the app on the Nano S (https://github.com/LedgerHQ/blue-loader-python)
Packages prerequisites: ```libudev-dev libusb-1.0-0-dev python-dev virtualenv```  
Virtualenv:
```
virtualenv ledger && source ledger/bin/activate && pip install ledgerblue==0.1.15
```

To load the app:
```
python -m ledgerblue.loadApp --appFlags 0x50 --path "44'/0'/0'" --curve secp256k1 --targetId 0x31100002 --fileName bin/app.hex --appName "Woleet" --icon '0100000000ffffff00c7e3f18f7dbe3e7c1ff837ec67e6c7e387e187e18ff19ff9be7dfdbff18fc7e3'
```

To remove the app:
```
python -m ledgerblue.deleteApp --targetId 0x31100002 --appName "Woleet"
```
