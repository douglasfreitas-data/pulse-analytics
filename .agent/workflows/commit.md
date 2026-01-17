---
description: Commit rápido com mensagem automática ou personalizada
---

# Workflow: Commit Rápido

## Uso
- `/commit` - Commit com mensagem automática baseada nos arquivos modificados
- `/commit <mensagem>` - Commit com mensagem personalizada

## Passos

// turbo-all

1. Verificar status do git:
```bash
cd /home/douglas/Documentos/Projects/PPG/pulse-analytics && git status --short
```

2. Adicionar todos os arquivos:
```bash
cd /home/douglas/Documentos/Projects/PPG/pulse-analytics && git add -A
```

3. Fazer o commit:
- Se o usuário passou uma mensagem, usar: `git commit -m "<mensagem>"`
- Se não passou mensagem, gerar uma baseada nos arquivos modificados

4. Push para origin:
```bash
cd /home/douglas/Documentos/Projects/PPG/pulse-analytics && git push origin main
```

## Notas
- O git já está configurado com:
  - user.name: douglasfreitas-data
  - user.email: douglas.freitas.data@gmail.com
- Para push automático, as credenciais devem estar em cache ou usar SSH key
