#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Inclusões dos headers do seu projeto.
#include "adsb_auxiliars.h"
#include "adsb_lists.h"
#include "adsb_time.h"
#include "adsb_decoding.h"
#include "adsb_createLog.h"
// Se você estiver usando o banco de dados, inclua também:
// #include "adsb_db.h"

// Variável global para armazenar a lista de mensagens (como no collector).
adsbMsg *messagesList = NULL;

int main() {
    // Para fins de teste, crie um pequeno conjunto de mensagens ADS-B válidas ou quase válidas.
    // Cada mensagem tem 28 caracteres hexadecimais. Estas abaixo são apenas exemplos.
    // Idealmente, você usaria mensagens que sabe que decodificam algo.
    const char *testMessages[] = {
    "8D76CE88204C9072CB48209A504D",
    "8D7C7181215D01A08208204D8BF1",
    "8D7C7745226151A08208205CE9C2",
    "8D7C80AD2358F6B1E35C60FF1925",
    "8D7C146525446074DF5820738E90",
    "8C7C1474381DA443C6450A369656",
    "8C7C451C423C52D692D953855472",
    "8D89611348DB01C6EA41C4C7B8BF",
    "8D7C1BE8581B66E9BD8CEEDC1C9F",
    "8F7C629659A0A6F64D8BAA09D3F0",
    "8D7C431FBF59D00000000069B618",
    "8D7C4A09E00300020259A0CBC2F4",
    "8D7C7181E1050B00000000C13340",
    "8D7C3F18E8000020713800DA52FD",
    "8D7C1474E9481900093810E5B315",
    "8DA8BBE7EA0BD870013F082BC5C5",
    "8D7C7181F80000060049A8F9E70F",
    "8C7C4A0CF9004103834938E42BD4"
    };
    int numTestes = sizeof(testMessages) / sizeof(testMessages[0]);

    // Vamos usar para armazenar a mensagem decodificada.
    char buffer[29]; 
    buffer[28] = '\0';

    // Ponteiro para o nó retornado pela decodeMessage.
    adsbMsg *node = NULL;

    // Inicia logs (se quiser usar)
    LOG_add("adsb_simulation", "Iniciando simulação de ADS-B...");

    // Loop para enviar as mensagens de teste ao decoder
    for(int i = 0; i < numTestes; i++){
        // Copia a mensagem de teste para buffer (simulando “chegada” de 28 bytes hex)
        strncpy(buffer, testMessages[i], 28);
        buffer[28] = '\0';

        printf("\n=== Teste %d: Mensagem = %s ===\n", i+1, buffer);

        // Chama decodeMessage. Note que não estamos fazendo checagem de CRC aqui.
        // Se quiser, pode usar a função CRC_tryMsg() antes, igual no collector.
        messagesList = decodeMessage(buffer, messagesList, &node);

        // Se conseguiu decodificar e retornou um `node` não-nulo:
        if(node != NULL) {
            // Verifica se as infos mínimas estão completas (posição, altitude, etc)
            if(isNodeComplete(node) != NULL) {
                // Aqui, apenas imprimimos. Se quisesse salvar no banco, chamaria DB_saveData(node).
                printf(">> Informações completas da aeronave %s:\n", node->ICAO);
                printf("   Callsign: %s\n", node->callsign);
                printf("   Latitude: %f\n", node->Latitude);
                printf("   Longitude: %f\n", node->Longitude);
                printf("   Altitude: %d ft\n", node->Altitude);
                printf("   Vel. Horizontal: %f nós (ou m/s, depende)\n", node->horizontalVelocity);
                printf("   Heading: %f graus\n", node->groundTrackHeading);

                // Se quiser “limpar” para não reutilizar dados antigos, use:
                clearMinimalInfo(node);
            } else {
                printf(">> Informações ainda não completas para %s.\n", node->ICAO);
            }
        } else {
            printf(">> decodeMessage não retornou nó válido.\n");
        }

        // Limpa o buffer para o próximo teste
        memset(buffer, 0, 29);
        node = NULL;
    }

    // Ao final, se desejar, podemos exibir a lista de aeronaves que temos armazenadas.
    printf("\n\n========== Lista final de aeronaves armazenadas ==========\n");
    adsbMsg *p = messagesList;
    while(p) {
        printf("ICAO = %s | Callsign = %s | Lat = %f | Lon = %f | Alt = %d\n",
                p->ICAO, p->callsign, p->Latitude, p->Longitude, p->Altitude);
        p = p->next;
    }

    // Liberar memória da lista
    LIST_removeAll(&messagesList);

    // Finaliza
    printf("Encerrando simulador ADS-B.\n");
    return 0;
}
