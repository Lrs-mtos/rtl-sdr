# RadarLivre C Collector

This algorithm is an ADS-B based message decoder written in C language, which communicates with a [micro ADS-B receptor](http://www.microadsb.com/), decodes the incoming messages and saves them in a local database. This project makes part of the RadarLivre project, developed by students and professors of Federal University of Ceara, campus Quixada.

## Files
This project contains the following files:
- **adsb_auxiliars(.c .h)**: this file has the auxiliary functions that are used for conversion, formatting, calculation and CRC operations.
- **adsb_decoding(.c .h)**: this file has the functions responsible for decode the incoming ADS-B messagens, getting the *ICAO address*, *callsign*, *latitude*, *longitude*, *altitude*, *horizontal velocity*, *vertical velocity* and *heading*.
- **adsb_lists(.c .h)**: this file has the functions responsible for list operations. The list is used to temporarily store the decoded ADS-B information.
- **adsb_serial(.c .h)**: the functions of this file are responsible for configuring and performing the serial communication operations, which are used to communicate with the micro ADS-B receptor.
- **adsb_time(.c .h)**: this file has the functions responsible for time reading and formatting, and for interrupt and timer configuration.
- **adsb_createLog(.c .h)**: this file has the functions responsible for create logs about the system.
- **adsb_db(.c .h)**: this file has the functions responsible for database operations. More specific, for initializing and saving operations.
- **adsb_userInfo.h**: this file has the user information that will be used to communicate with a remote server.
- **adsb_collector.c**: this file has the main function.

### Database
The database used in this version is the [SQLite](https://www.sqlite.org/index.html) 3.28.0. We use two main tables: **radarlivre_api_adsbinfo** and **radarlivre_api_airline**. Their schematic can be saw below:

```sh
CREATE TABLE "radarlivre_api_adsbinfo" 
(
	"id" integer NOT NULL PRIMARY KEY AUTOINCREMENT, 
	"collectorKey" varchar(64) NULL, 
	"modeSCode" varchar(16) NULL, 
	"callsign" varchar(16) NULL, 
	"latitude" decimal NOT NULL, 
	"longitude" decimal NOT NULL, 
	"altitude" decimal NOT NULL, 
	"verticalVelocity" decimal NOT NULL, 
	"horizontalVelocity" decimal NOT NULL, 
	"groundTrackHeading" decimal NOT NULL, 
	"timestamp" bigint NOT NULL, 
	"timestampSent" bigint NOT NULL, 
	"messageDataId" varchar(100) NOT NULL, 
	"messageDataPositionEven" varchar(100) NOT NULL, 
	"messageDataPositionOdd" varchar(100) NOT NULL, 
	"messageDataVelocity" varchar(100) NOT NULL
);
```

```sh
CREATE TABLE "radarlivre_api_airline"
(
	"id" integer NOT NULL PRIMARY KEY AUTOINCREMENT, 
	"name" varchar(255) NULL,
	"alias" varchar(255) NULL,
	"iata" varchar(4) NULL,
	"icao" varchar(8) NULL,
	"callsign" varchar(255) NULL,
	"country" varchar(255) NULL,
	"active" bool NULL	
);
```

## Compiling and Running

To compile the system, we use the Makefile. As compiler, we are using the **gcc** and as a cross-compiler we are using the **arm-linux-gnueabihf-gcc**. To compile, it is just necessary to execute the make command, as below:
```sh
make
```
And to clean the project, we execute:
```sh
make clean
```
After the compiling, a file called **run_collector** will be generated. To run the system, we run this file, as below:
```sh
sudo ./run_collector
```
When running the system, two files will be generated: **radarlivre_v4.db**, which is the database file, and **adsb_log.log**, which is the log file.


# **Guia de Instalação e Configuração do Prometheus + SQLite Exporter no Orange Pi**

> **Autor:** Radarlivre  
> **Dispositivo:** Orange Pi  
> **IP Principal:** `192.168.0.8`  
> **IP do Orange Pi:** `<IP_DA_ORANGE_PI>`  

---

## **1. Atualizando o Sistema**
```sh
sudo apt update && sudo apt upgrade -y
```

---

## **2. Instalando Dependências**
```sh
sudo apt install -y git sqlite3 libsqlite3-dev build-essential cmake prometheus prometheus-node-exporter
```

---

## **3. Criando o Script do SQLite Exporter**
Crie o script que coleta métricas do banco de dados e envia para o Prometheus.

```sh
nano /home/orangepi/sqlite_exporter.sh
```
**📄 Conteúdo do script (`sqlite_exporter.sh`):**
```sh
#!/bin/bash
DB_PATH="/home/orangepi/radarlivre_v4.db"

# Total de mensagens no banco
TOTAL_REGISTROS=$(sqlite3 $DB_PATH "SELECT COUNT(*) FROM radarlivre_api_adsbinfo;")

# Média de altitude das aeronaves
MEDIA_ALTITUDE=$(sqlite3 $DB_PATH "SELECT AVG(altitude) FROM radarlivre_api_adsbinfo;")

# Velocidade média horizontal das aeronaves
MEDIA_VELOCIDADE=$(sqlite3 $DB_PATH "SELECT AVG(horizontalVelocity) FROM radarlivre_api_adsbinfo;")

# Quantidade de aeronaves distintas
AERONAVES_DISTINTAS=$(sqlite3 $DB_PATH "SELECT COUNT(DISTINCT modeSCode) FROM radarlivre_api_adsbinfo;")

# Dados da CPU (quando implementado)
CPU_USAGE=$(cat /proc/loadavg | awk '{print $1}')

# Formata saída para Prometheus
echo "# HELP adsb_total_messages Número total de mensagens ADS-B coletadas"
echo "# TYPE adsb_total_messages counter"
echo "adsb_total_messages $TOTAL_REGISTROS"

echo "# HELP adsb_average_altitude Altitude média das aeronaves"
echo "# TYPE adsb_average_altitude gauge"
echo "adsb_average_altitude $MEDIA_ALTITUDE"

echo "# HELP adsb_average_speed Velocidade média horizontal das aeronaves"
echo "# TYPE adsb_average_speed gauge"
echo "adsb_average_speed $MEDIA_VELOCIDADE"

echo "# HELP adsb_unique_aircraft Número de aeronaves únicas"
echo "# TYPE adsb_unique_aircraft gauge"
echo "adsb_unique_aircraft $AERONAVES_DISTINTAS"

echo "# HELP system_cpu_usage Uso médio da CPU"
echo "# TYPE system_cpu_usage gauge"
echo "system_cpu_usage $CPU_USAGE"
```
**Salvar e sair** (`CTRL + X`, `Y`, `Enter`)

**Dar permissão de execução:**
```sh
chmod +x /home/orangepi/sqlite_exporter.sh
```

---

## **4. Configurando o Prometheus**
Edite o arquivo de configuração do **Prometheus**:
```sh
sudo nano /etc/prometheus/prometheus.yml
```
**Conteúdo do arquivo (`prometheus.yml`):**
```yaml
global:
  scrape_interval: 15s
  evaluation_interval: 15s

scrape_configs:
  # Prometheus local (no Orange Pi)
  - job_name: 'prometheus_orangepi'
    static_configs:
      - targets: ['localhost:9090']

  # SQLite Exporter do Orange Pi
  - job_name: 'sqlite_exporter'
    metrics_path: '/metrics'
    static_configs:
      - targets: ['localhost:9100']

  # Node Exporter do Orange Pi (para métricas de sistema)
  - job_name: 'node_exporter'
    static_configs:
      - targets: ['localhost:9101']

# Enviando dados para o Prometheus principal (192.168.0.8)
remote_write:
  - url: "http://192.168.0.8:9090/api/v1/write"
remote_read:
  - url: "http://192.168.0.8:9090/api/v1/read"
```
**Salvar e sair** (`CTRL + X`, `Y`, `Enter`)

**Reiniciar o Prometheus:**
```sh
sudo systemctl restart prometheus
```

---

## **5. Criando o Serviço `sqlite_exporter`**
Agora, vamos criar um serviço para rodar o `sqlite_exporter.sh` automaticamente.

```sh
sudo nano /etc/systemd/system/sqlite_exporter.service
```
**Conteúdo do arquivo (`sqlite_exporter.service`):**
```ini
[Unit]
Description=SQLite Exporter para Prometheus
After=network.target

[Service]
ExecStart=/bin/bash /home/orangepi/sqlite_exporter.sh
Restart=always
User=orangepi

[Install]
WantedBy=multi-user.target
```
**Salvar e sair** (`CTRL + X`, `Y`, `Enter`)

**Ativar e iniciar o serviço:**
```sh
sudo systemctl daemon-reload
sudo systemctl enable sqlite_exporter
sudo systemctl start sqlite_exporter
```

---

## **6. Instalando o Grafana**
```sh
echo "deb https://packages.grafana.com/oss/deb stable main" | sudo tee -a /etc/apt/sources.list.d/grafana.list
sudo apt install -y software-properties-common
sudo wget -q -O - https://packages.grafana.com/gpg.key | sudo apt-key add -
sudo apt update
sudo apt install -y grafana
```
**Iniciar e ativar o Grafana:**
```sh
sudo systemctl enable --now grafana-server
sudo systemctl status grafana-server
```

---

## **7. Acessar o Grafana**
1. **Abra o navegador e acesse:**  
   ```
   http://<IP_DA_ORANGE_PI>:3000
   ```
2. **Login padrão:**  
   ```
   Usuário: admin
   Senha: radarlivre
   ```
3. **Configurar a fonte de dados:**  
   - **Configuration > Data Sources > Add Data Source**
   - Escolha **Prometheus**
   - **URL:** `http://192.168.0.8:9090`
   - **Save & Test**

Agora, **o Grafana no Orange Pi está enviando métricas para o Prometheus principal!** 
