#ifndef CONFIG_SECRETS_EXAMPLE_H
#define CONFIG_SECRETS_EXAMPLE_H

/*
 * ==============================================================================
 * TEMPLATE DE EXEMPLO PARA CREDENCIAIS (SEGURO PARA COMMITAR NO GIT)
 * ==============================================================================
 * Para usar em ambiente real/produção:
 *   1. Copie este arquivo para 'config_secrets.h' (ou copie para 'src/config_secrets.h')
 *   2. Preencha com seu SSID, senha de Wi-Fi e chave de API reais.
 *   3. O arquivo 'config_secrets.h' é ignorado pelo .gitignore e nunca subirá ao Git.
 */

// Credenciais da Rede Wi-Fi
#define SECRET_WIFI_SSID            "SUA_REDE_WIFI"
#define SECRET_WIFI_PASS            "SUA_SENHA_WIFI"

// Identificador do Dispositivo e Chaves da API Vitae
#define SECRET_SYSTEM_ID            "MOQA-INFRA-001"
#define SECRET_VITAE_API_KEY        "mqk_live_coloque_aqui_seu_token_privado"
#define SECRET_VITAE_API_URL        "https://api.exemplo.gov.br/telemetry"
#define SECRET_VITAE_API_URL_BACKUP "https://api-backup.exemplo.gov.br/telemetry"

#endif // CONFIG_SECRETS_EXAMPLE_H
