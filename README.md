# Lottery Game: Client-Server implementation

**LotteryGame** is a simple Client-Server Lottery Simulator playable just through the terminal. It handles multiple session by managing multiple TCP connections. This project is an assignement for the **Computer Networks' exam** (Reti Informatiche) of the BSc in Computer Engineering (Ingegneria Informatica) at the University of Pisa.    
The following project was ran and tested on Debian 8 distribution. 


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


[![Hits](https://hits.seeyoufarm.com/api/count/incr/badge.svg?url=https%3A%2F%2Fgithub.com%2Fgerti98%2FCar-Sharing-Database&count_bg=%2379C83D&title_bg=%23555555&icon=&icon_color=%23E7E7E7&title=hits&edge_flat=false)](https://hits.seeyoufarm.com)
