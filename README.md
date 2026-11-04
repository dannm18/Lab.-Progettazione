Il progetto consiste nel programmare una scheda, STM32-NUCLEO, basata sul microcontrollore STM32-F401RE, per controllare un LED a seconda del numero di pressioni del tasto USER presente sulla scheda stessa. 
Progetto
Premendo il tasto USER, al rilascio il LED integrato sulla scheda lampeggia con un periodo di 2 secondi e un duty cycle del 30% per tre volte e resta acceso. Se nei successivi 3 secondi il tasto viene premuto per due volte consecutive, il LED lampeggia 4 volte e poi si spegne tornando allo stato iniziale. Tramite UART si stampano gli stati del LED e le pressioni del tasto.
Specifiche
Utilizziamo una scheda di sviluppo STM32-NUCLEO F401RE, il led verde integrato connesso al PIN GPIO C13, la scheda è connessa al PC e tramite protocollo USART leggiamo la seriale a 115200 baud.
