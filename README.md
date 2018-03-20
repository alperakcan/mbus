# mBus #

mBus is a lightweight messaging system providing communication between daemons
and applications. enabling development of component based distributed applications.

1. <a href="#1-overview">overview</a>
2. <a href="#2-download">download</a>
3. <a href="#3-build">build</a>
4. <a href="#4-applications">applications</a>
    - <a href="#41-broker">broker</a>
        - <a href="#411-command-line-options">command line options</a>
    - <a href="#42-subscribe">subscribe</a>
        - <a href="#421-command-line-options">command line options</a>
    - <a href="#43-publish">publish</a>
        - <a href="#431-command-line-options">command line options</a>
    - <a href="#44-command">command</a>
        - <a href="#441-command-line-options">command line options</a>
5. <a href="#5-library">library</a>
    - <a href="#51-server">server</a>
    - <a href="#52-client">client</a>
6. <a href="#6-protocol">protocol</a>
7. <a href="#7-tls">tls</a>
    - <a href="#71-certificate-authority">certificate authority</a>
    - <a href="#72-server">server</a>
    - <a href="#73-client">client</a>
8. <a href="#8-tests">tests</a>
    - <a href="#81-logger">logger</a>
9. <a href="#9-contact">contact</a>
10. <a href="#10-license">license</a>

## 1. overview ##

mBus is a lightweight messaging system providing communication between daemons 
and applications. enabling development of component based distributed
applications.

every daemon registers with a unique namespace and a set of procedures with
various number of arguments under the namespace. procedure request and responce
are in JSON format. any daemon can post an event under registered namespace
with various number of arguments in JSON format. any daemon or application can
subscribe to events with namespaces and can call procedures from other
namespaces.

mBus consists of two parts; a server and client[s]. server interface for other
daemons/applications to register themselves, send events and call procedures from
other daemons/applications.

all messaging is in JSON format, and communication is based on TLV (type-length-value)
messages.

mBus framework has built in applications; broker, subscribe, command and publish.
broker is a very simple implementation of server and is the hearth of mBus.
subscribe is to listen every messages generated by either server or clients, and
useful for debug purposes. publish is for publishing an event, and command is for
calling a procedure from connected clients.

mBus client library (libmbus-client) is to simplify development of software using mbus
(connecting to it).

## 2. download ##

    git clone --recursive git@github.com:alperakcan/mbus.git

or

    git clone git@github.com:alperakcan/mbus.git
    cd mbus
    git submodule update --init --recursive

## 3. build ##

    sudo apt install gcc
    sudo apt install make
    sudo apt install pkg-config
    sudo apt install libssl-dev
    sudo apt install zlib1g-dev
    sudo apt install libwebsockets-dev
    sudo apt install libreadline-dev

    cd mbus
    make

## 4. applications ##

### 4.1 broker ###

#### 4.1.1 command line options ####

  - --mbus-help
  
    print available parameters list and exit

  - --mbus-debug-level

    set debug level, available options: debug, info, error. default: error
  
  - --mbus-server-tcp-enable
  
    server tcp enable, default: 1
  
  - --mbus-server-tcp-address
  
    server tcp address, default: 127.0.0.1
  
  - --mbus-server-tcp-port
  
    server tcp port, default: 8000
  
  - --mbus-server-uds-enable
  
    server uds enable, default: 1
  
  - --mbus-server-uds-address
  
    server uds address, default: /tmp/mbus-server-uds
  
  - --mbus-server-uds-port
  
    server uds port, default: -1
  
  - --mbus-server-ws-enable
  
    server websocket enable, default: 1
    
  - --mbus-server-ws-address
  
    server websocket address, default: 127.0.0.1
  
  - --mbus-server-ws-port
  
    server websocket port, default: 9000
  
  - --mbus-server-tcps-enable
  
    server tcps enable, default: 1
  
  - --mbus-server-tcps-address
  
    server tcps address, default: 127.0.0.1
  
  - --mbus-server-tcps-port
  
    server tcps port, default: 8000

  - --mbus-server-tcps-certificate
  
    server tcps certificate (default: server.crt)
  
  - --mbus-server-tcps-privatekey
  
    server tcps privatekey (default: server.key)
  
  - --mbus-server-udss-enable
  
    server udss enable, default: 1
  
  - --mbus-server-udss-address
  
    server udss address, default: /tmp/mbus-server-udss
  
  - --mbus-server-udss-port
  
    server udss port, default: -1
  
  - --mbus-server-udps-certificate
  
    server udps certificate (default: server.crt)
  
  - --mbus-server-udps-privatekey
  
    server udps privatekey (default: server.key)
  
  - --mbus-server-wss-enable
  
    server websocket enable, default: 1
    
  - --mbus-server-wss-address
  
    server websocket address, default: 127.0.0.1
  
  - --mbus-server-wss-port
  
    server websocket port, default: 9000
  
  - --mbus-server-wss-certificate
  
    server wss certificate (default: server.crt)
  
  - --mbus-server-wss-privatekey
  
    server wss privatekey (default: server.key)
  
### 4.2 subscribe ###

#### 4.2.1 command line options ####

  - --mbus-help
  
    print available parameters list and exit

  - --mbus-debug-level

    set debug level, available options: debug, info, error. default: error
  
  - --mbus-server-protocol
  
    set communication protocol, available options: tcp, uds, tcps, udss. default: uds

  - --mbus-server-address
  
    set server address:

    - tcp: default is 127.0.0.1
    - uds: default is /tmp/mbus-server
  
  - --mbus-server-port
  
    set server port:

    - tcp: default is 8000
    - uds: unused

  - --mbus-client-name
  
    set client name, default: org.mbus.app.subscribe

  - --mbus-ping-interval

    ping interval. default: 180000

  - --mbus-ping-timeout

    ping timeout. default: 5000

  - --mbus-ping-threshold

    ping threshold. default: 2

### 4.3 publish ###

#### 4.3.1 command line options ####

  - --mbus-help
  
    print available parameters list and exit

  - --mbus-debug-level

    set debug level, available options: debug, info, error. default: error
  
  - --mbus-server-protocol
  
    set communication protocol, available options: tcp, uds, tcps, udss. default: uds

  - --mbus-server-address
  
    set server address:

    - tcp: default is 127.0.0.1
    - uds: default is /tmp/mbus-server
  
  - --mbus-server-port
  
    set server port:

    - tcp: default is 8000
    - uds: unused

  - --mbus-client-name
  
    set client name, default: org.mbus.app.cli

  - --mbus-ping-interval

    ping interval. default: 180000

  - --mbus-ping-timeout

    ping timeout. default: 5000

  - --mbus-ping-threshold

    ping threshold. default: 2

### 4.4 command ###

#### 4.4.1 command line options ####

  - --mbus-help
  
    print available parameters list and exit

  - --mbus-debug-level

    set debug level, available options: debug, info, error. default: error
  
  - --mbus-server-protocol
  
    set communication protocol, available options: tcp, uds, tcps, udss. default: uds

  - --mbus-server-address
  
    set server address:

    - tcp: default is 127.0.0.1
    - uds: default is /tmp/mbus-server
  
  - --mbus-server-port
  
    set server port:

    - tcp: default is 8000
    - uds: unused

  - --mbus-client-name
  
    set client name, default: org.mbus.app.cli

  - --mbus-ping-interval

    ping interval. default: 180000

  - --mbus-ping-timeout

    ping timeout. default: 5000

  - --mbus-ping-threshold

    ping threshold. default: 2

## 5. library ##

### 5.1 server ###

### 5.2 client ###

## 6. protocol ##

mbus broker supports tcp, uds, and websockets connections.

## 7. tls ##

mbus provides ssl support for encrypted network connections and authentication

### 7.1 certificate authority ###

generate a certificate authority certificate and key

    openssl req -new -x509 -days 365 -extensions v3_ca -keyout ca.key -out ca.crt

### 7.2 server ###

generate server key

    openssl genrsa -des3 -out server.key 2048

generate server key without encryption

    openssl genrsa -out server.key 2048

generate a certificate signing request to send to the ca

    openssl req -out server.csr -key server.key -new -newkey rsa:4096 -sha256

send the csr to the ca, or sign it with you ca key

    openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt -days 365

### 7.3 client ###

generate client key

    openssl genrsa -des3 -out client.key 2048

generate a certificate signing request to send to the ca

    openssl req -out client.csr -key client.key -new

send the csr to the ca, or sign it with you ca key

    openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365

## 8. tests ##

### 8.1 logger ###

build & install

    git clone https://github.com/alperakcan/mbus.git mbus.git
    cd mbus.git
    git submodule update --init --recursive
    make -j
    sudo make install
    sudo ldconfig

run

    mbus-broker --help

create certificate authority, enter password when requested

    openssl req -new -x509 -days 365 -extensions v3_ca -keyout mbus-ca.key -out mbus-ca.crt

generate server certificate and key, do not enter any password

    openssl genrsa -out mbus-server.key 2048
    openssl req -out mbus-server.csr -key mbus-server.key -new -newkey rsa:4096 -sha256

add csr to the ca, enter password used for ca authority

    openssl x509 -req -in mbus-server.csr -CA mbus-ca.crt -CAkey mbus-ca.key -CAcreateserial -out mbus-server.crt -days 365

execute broker with ssl support, disable non-ssl listeners

    mbus-broker \
      --mbus-debug-level info \
      --mbus-server-tcp-enable 0 --mbus-server-tcp-address 0.0.0.0 --mbus-server-tcp-port 8000 \
      --mbus-server-uds-enable 0 --mbus-server-uds-address /tmp/mbus-server-uds --mbus-server-uds-port 0 \
      --mbus-server-ws-enable 0 --mbus-server-ws-address 0.0.0.0 --mbus-server-ws-port 9000 \
      --mbus-server-tcps-enable 1 --mbus-server-tcps-address 0.0.0.0 --mbus-server-tcps-port 8001 --mbus-server-tcps-certificate mbus-server.crt --mbus-server-tcps-privatekey mbus-server.key \
      --mbus-server-udss-enable 1 --mbus-server-udss-address /tmp/mbus-server-udss --mbus-server-udss-port 0 --mbus-server-udss-certificate mbus-server.crt --mbus-server-udss-privatekey mbus-server.key \
      --mbus-server-wss-enable 1 --mbus-server-wss-address 0.0.0.0 --mbus-server-wss-port 9001 --mbus-server-wss-certificate mbus-server.crt --mbus-server-wss-privatekey mbus-server.key

publish

    mbus-test-logger-publish --help

    mbus-test-logger-publish \
      --qos 1 \
      --mbus-client-identifier org.mbus.client.test.logger.publish.0 \
      --mbus-client-server-protocol tcps --mbus-client-server-address 104.236.206.233 --mbus-client-server-port 8001 \
      --mbus-client-connect-timeout 30000 --mbus-client-connect-interval 5000 \
      --mbus-client-ping-interval 10000 --mbus-client-ping-timeout 5000 --mbus-client-ping-threshold 2

subscribe

    mbus-test-logger-subscribe --help

    mbus-test-logger-subscribe \
      --qos 1 \
      --calback 1 \
      --mbus-client-identifier org.mbus.client.test.logger.subscribe.0 \
      --mbus-client-server-protocol tcps --mbus-client-server-address 104.236.206.233 --mbus-client-server-port 8001 \
      --mbus-client-connect-timeout 30000 --mbus-client-connect-interval 5000 \
      --mbus-client-ping-interval 10000 --mbus-client-ping-timeout 5000 --mbus-client-ping-threshold 2

## 9. contact ##

if you are using the software and/or have any questions, suggestions, etc.
please contact with me at alper.akcan@gmail.com

## 10. license ##

### mBus Copyright (c) 2014-2018, Alper Akcan <alper.akcan@gmail.com> ###

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

  - Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  - Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

  - Neither the name of the developer nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
