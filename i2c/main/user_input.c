#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "driver/uart.h"
#include <stdio.h>
#include <ctype.h>
void get_user_input(bool* inv, int* cont, int* yof, int* xof){
setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, 256, 0, 0, NULL, 0));
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
    esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);
    char chr[100];
    while(1){
        printf("Czy chcesz wejsc w tryb konfiguracyjny? (y/n)\n");
        scanf("%1s", chr);
        if(strcasecmp(chr, "n") == 0){
            printf("Uruchomienie wyświetlacza w domyślnej konfiguracji... \n");
            return;
        }
        else if(strcasecmp(chr, "y") == 0){
            while(1){
                printf("Podaj parametr, ktory chcesz skonfigurowac: \ncontrast, inversion, yoffset, xoffset \nAby wyjść z trybu konfiguracyjnego wpisz: exit\n");
                scanf("%s", chr);
                if(strcasecmp(chr, "exit") == 0){
                    printf("Uruchamiam wyświetlacz...\n");
                    return;
                }
                else if(strcasecmp(chr, "inversion") == 0){
                    while(1){
                        printf("Czy chcesz odwrocic kolory na wyswietlaczu? (y/n) \n");
                        scanf("%1s", chr);
                        if(strcasecmp(chr, "y") == 0){
                            *inv = true;
                            printf("\nKolory zostaną odwrócone\n \n");
                            break;
                        }
                        else if(strcasecmp(chr, "n") == 0){
                            *inv = false;
                            printf("\nKolory nie zostaną odwrócone\n \n");
                            break;
                        }
                        else{
                            printf("Podaj y lub n!\n");
                        }
                    }
                }
                else if(strcasecmp(chr, "contrast") == 0){
                    while(1){
                        bool wrong_input = false;
                        printf("Podaj wartosc kontrastu:\n");
                        scanf("%s", chr);
                        for(int i =0; i<strlen(chr); i++){
                            if(isdigit((unsigned char)chr[i])==0){
                                printf("Podaj liczbę\n");
                                wrong_input = true;
                                break;
                            }
                        }
                        if(wrong_input){
                            continue;
                        }
                        *cont = atoi(chr);
                        printf("\nKontrast ustawiony na %s\n \n", chr);
                        break;
                    }
                }
                else if(strcmp(chr, "yoffset") == 0){
                    while(1){
                        bool wrong_input = false;
                        printf("Podaj, o ile pikseli w dół chcesz przesunąć tekst:\n");
                        scanf("%s", chr);
                        for(int i =0; i<strlen(chr); i++){
                            if(isdigit((unsigned char)chr[i])==0){
                                printf("Podaj liczbę\n");
                                wrong_input = true;
                                break;
                            }
                        }
                        if(wrong_input){
                            continue;
                        }
                        *yof = atoi(chr);
                        printf("\nPrzesunięcie w dół ustawione na %s pikseli\n \n", chr);
                        break;
                    }
                }
                else if(strcmp(chr, "xoffset") == 0){
                    while(1){
                        bool wrong_input = false;
                        printf("Podaj, o ile pikseli w prawo chcesz przesunąć tekst:\n");
                        scanf("%s", chr);
                        for(int i =0; i<strlen(chr); i++){
                            if(isdigit((unsigned char)chr[i])==0){
                                printf("Podaj liczbę\n");
                                wrong_input = true;
                                break;
                            }
                        }
                        if(wrong_input){
                            continue;
                        }
                        *xof = atoi(chr);
                        printf("\nPrzesunięcie w prawo ustawione na %s pikseli\n \n", chr);
                        break;
                    }
                }
            }
        }
        else{
            printf("Wprowadź \"y\" lub \"n\"?");
        }
    }
}
