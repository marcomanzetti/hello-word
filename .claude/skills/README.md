# Skill del progetto

Queste skill si caricano da sole in ogni sessione di Claude Code aperta su
questo repository.

| Skill | A cosa serve |
|---|---|
| `investor-setup` | configurazione iniziale e guida |
| `the-scout` | cerca idee su tutto il mercato USA |
| `the-judge` | dà un voto da 0 a 100 a un'azienda |
| `the-mind-reader` | cosa si aspetta già il prezzo (DCF inverso) |
| `the-sniper` | zone di acquisto (supporti) |
| `the-time-traveller` | 10.000 scenari per i prossimi 10 anni |

Fonte: https://github.com/wallstreetgpt/investor-skills (versione 1.4.0),
copiate senza modifiche. Sono strumenti di studio, non consigli di
investimento.

## Contatto per la SEC

La SEC vuole nome ed email in ogni richiesta. Per non scriverli nel
repository, si impostano come variabile d'ambiente nelle impostazioni
dell'ambiente cloud:

```
SEC_USER_AGENT=Nome Cognome email@esempio.com
```

In alternativa, in ogni nuova sessione si può dire "set up investor skills".
