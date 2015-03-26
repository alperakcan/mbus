# mBus #

mBus is a lightweight messaging system.

1. <a href="#1-overview">overview</a>
2. <a href="#2-download">download</a>
3. <a href="#3-build">download</a>
4. <a href="#4-execute">execute</a>
  - <a href="#41-controller">controller</a>
  - <a href="#42-launcher">launcher</a>
  - <a href="#43-listener">listener</a>
  - <a href="#44-cli">cli</a>
5. <a href="#5-api">api</a>
6. <a href="#6-protocol">protocol</a>
7. <a href="#7-contact">contact</a>
8. <a href="#8-license">license</a>

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

## 2. download ##

## 3. build ##

## 4. execute ##

### 4.1 controller ###

### 4.2 launcher ###

### 4.3 listener ###

### 4.4 cli ###

## 5. api ##

### 5.1 server ###

### 5.2 client ###

## 6. protocol ##

## 7. contact ##

## 8. license ##

### mBus Copyright (c) 2014, Alper Akcan <alper.akcan@gmail.com> ###

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

### cJSON Copyright (c) 2009 Dave Gamble ###
 
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
 
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
 
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

