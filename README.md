# LotteryGame 

**LotteryGame** is a simple Client-Server Lottery Simulator playable just through the terminal. It handles multiple session by managing multiple TCP connections.
You'll need a Unix-based operating system to run it.


![Client Example](https://github.com/gerti98/LotteryGame/blob/master/images/client_example.png)


## How to use
  1. Clone the repository and through the terminal enter in the same downloaded directory and type: `> make`
  2. Firstly run the server by typing:
  
      `./lotto_server *port_number* [*period*]`     
  > port_number: port which identifies the process of the server      
  > period: time (in minutes) to wait before each extraction
  3. Then open a new Terminal and run the client by typing:
  
      `./lotto_client *ip_address_server* *port_number*`     
  > port_number: port which identifies the process of the server (must be the same)      
  > ip_address_server: ip address (IPv4) of the server, in this example will be `127.0.0.1` (because we are running client and server in the same host
  4. You can now type `!help` to watch which commands are available. You must register and then log-in: you can send to the server your bet, watch past extractions, show your past wins and so on.

NOTE: this tool shows messages only in Italian. I'm currently working on a English translation to make everything more comprehensive. For now you can relay on Google Translate instead.
