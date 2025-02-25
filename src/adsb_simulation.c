#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Inclusões dos headers do projeto
#include "adsb_auxiliars.h"
#include "adsb_lists.h"
#include "adsb_time.h"
#include "adsb_decoding.h"
#include "adsb_createLog.h"
#include "adsb_db.h"
#include "board_monitor.h"

// Ponteiro global para a lista de mensagens ADS-B
adsbMsg *messagesList = NULL;

/*==============================================
FUNCTION: main
INPUT: none
OUTPUT: integer exit status
DESCRIPTION: Este programa de simulação envia um conjunto de mensagens ADS‑B (strings de 28 hex)
para o decodificador. Para cada mensagem, chama decodeMessage() para atualizar a lista.
Independente de os dados estarem completos ou não, DB_saveData() é chamada para salvar os dados
no banco, e também são coletadas métricas de CPU para salvar na tabela system_metrics.
================================================*/
int main(void) {
    // Array de mensagens de teste (28 caracteres hexadecimais)
    const char *testMessages[] = {
        "88F0984044000000000000000000"
    };
    int numTests = sizeof(testMessages) / sizeof(testMessages[0]);

    char buffer[29];
    buffer[28] = '\0';
    adsbMsg *node = NULL;

    // Log de início da simulação
    LOG_add("adsb_simulation", "Iniciando simulação de ADS-B...");

    // Processa cada mensagem de teste
    for (int i = 0; i < numTests; i++) {
        // Simula a recepção de 28 bytes hex
        strncpy(buffer, testMessages[i], 28);
        buffer[28] = '\0';

        printf("\n=== Teste %d: Mensagem = %s ===\n", i + 1, buffer);

        // Chama o decodificador para atualizar a lista de mensagens
        messagesList = decodeMessage(buffer, messagesList, &node);
        if (node != NULL) {
            LOG_add("adsb_simulation", "Successfully decoded a message (node != NULL)");
            // Agora, mesmo que o nó esteja incompleto, salvamos os dados no BD.
            LOG_add("adsb_simulation", "Calling DB_saveData (saving even with null values)");
            int ret = DB_saveData(node);
            
            // Coleta métricas do sistema (CPU, memória) e salva no BD
            double user_cpu, sys_cpu;
            long max_rss;
            getCpuUsage(&user_cpu, &sys_cpu, &max_rss);
            DB_saveSystemMetrics(user_cpu, sys_cpu, max_rss);
            
            if (ret != 0) {
                printf(">> Falha ao salvar informações para %s.\n", node->ICAO);
            } else {
                printf(">> Dados da aeronave %s salvos com sucesso!\n", node->ICAO);
                clearMinimalInfo(node);
            }
        } else {
            printf(">> decodeMessage não retornou nó válido.\n");
        }
        memset(buffer, 0, 29);
        node = NULL;
    }

    // Exibe a lista final de aeronaves armazenadas (em memória)
    printf("\n========== Lista final de aeronaves armazenadas ==========\n");
    adsbMsg *p = messagesList;
    while (p) {
        printf("ICAO = %s | Callsign = %s | Lat = %f | Lon = %f | Alt = %d\n",
               p->ICAO, p->callsign, p->Latitude, p->Longitude, p->Altitude);
        p = p->next;
    }

    // Libera a memória e registra o fim da simulação
    LIST_removeAll(&messagesList);
    LOG_add("adsb_simulation", "Simulação de ADS-B encerrada");
    printf("Encerrando simulador ADS-B.\n");

    return 0;
}
