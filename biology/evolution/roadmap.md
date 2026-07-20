# Roteiro de Desenvolvimento - Simulação de Evolução

Este documento descreve um plano para o desenvolvimento incremental da simulação de evolução, partindo da estrutura básica atual para um ecossistema digital mais complexo e visualmente informativo.

---

## Fase 1: Mecânicas Fundamentais da Simulação

O objetivo desta fase é criar um ciclo de vida básico para as criaturas ("critters") e estabelecer as interações fundamentais com o ambiente.

1.  **Geração Inicial do Mundo:**
    -   Em `simulation.cpp`, implementar `ResetSimulation()` para popular o mundo com uma quantidade inicial de `critters` e `foods` em posições aleatórias dentro de uma área definida (ex: um plano de -50 a +50 em X e Z).

2.  **Atributos e Energia dos Critters:**
    -   Expandir a struct `Critter` em `simulation.h` para incluir atributos básicos:
        ```c++
        struct Critter {
            float x, y, z;
            float energy = 100.0f;
            float speed = 5.0f;
            float sightRadius = 20.0f;
        };
        ```

3.  **Comportamento Básico (IA):**
    -   Em `UpdateSimulation()`, implementar a lógica de movimento:
        -   **Busca por Comida:** Se a energia estiver baixa, o critter deve procurar pelo `Food` mais próximo dentro do seu `sightRadius` e se mover em sua direção.
        -   **Movimento Aleatório:** Se não houver comida por perto ou se a energia estiver alta, o critter se move aleatoriamente.
        -   **Consumo de Energia:** O movimento consome `energy`. A quantidade gasta pode ser proporcional à `speed`.

4.  **Ciclo de Vida e Morte:**
    -   **Alimentação:** Quando um critter alcança um `Food`, o `Food` é removido e a `energy` do critter é restaurada.
    -   **Morte:** Se a `energy` de um critter chegar a zero, ele é removido da simulação.

5.  **Geração de Comida:**
    -   Implementar um mecanismo em `UpdateSimulation()` para gerar novos `Food` periodicamente, garantindo que o ecossistema não morra por falta de recursos.

---

## Fase 2: Genética e Evolução

Com as mecânicas básicas funcionando, esta fase introduz a hereditariedade e a mutação, que são os pilares da evolução por seleção natural.

1.  **Código Genético:**
    -   Criar uma `struct Genes` para armazenar as características hereditárias.
    -   Mover os atributos variáveis (`speed`, `sightRadius`, etc.) da `struct Critter` para a `struct Genes`. Adicionar também a cor.
        ```c++
        struct Genes {
            float speed;
            float sightRadius;
            float r, g, b; // Cor
        };
        struct Critter {
            // ...
            Genes genes;
        };
        ```

2.  **Reprodução:**
    -   Quando um critter acumula energia acima de um certo limiar, ele se reproduz.
    -   **Modelo Asexual:** Inicialmente, a reprodução pode ser assexual. O critter cria um "filho" com uma cópia de seus genes. A energia do pai é dividida com o filho.

3.  **Mutação:**
    -   Durante a reprodução, introduzir uma pequena chance de mutação nos genes do filho. Por exemplo, a `speed` do filho pode ser a `speed` do pai +/- um valor aleatório pequeno.
    -   A mutação é a fonte de novas características na população, permitindo a adaptação.

---

## Fase 3: Melhorias na Visualização e Interface (HUD)

O objetivo é tornar a simulação mais informativa e fácil de entender visualmente.

1.  **Renderização Melhorada:**
    -   Em `render.cpp`, desenhar os `critters` como formas 3D simples (esferas ou cubos) em vez de pontos.
    -   A cor de cada critter deve ser definida pelos seus genes (`genes.r`, `genes.g`, `genes.b`).
    -   Desenhar um plano no chão para dar uma melhor noção de espaço e movimento.

2.  **Criação do HUD:**
    -   Criar `hud.h` e `hud.cpp` (usando `sistem/main.cpp` como referência para a criação de fontes e texto).
    -   Exibir estatísticas em tempo real na tela:
        -   População atual de critters.
        -   Quantidade de comida.
        -   Velocidade da simulação (1x, 2x, ...).
        -   Status (Rodando / Pausado).
        -   Média dos genes da população (velocidade média, raio de visão médio).

3.  **Interatividade e Informação:**
    -   Implementar a seleção de um critter com o mouse.
    -   Ao selecionar um critter, exibir suas informações individuais no HUD (energia, genes específicos).
    -   Adicionar um "Painel de Ajuda" (como em `visualOrdering`) que explique os controles (`R` para resetar, `ESPAÇO` para pausar, `H` para ajuda, etc.).

---

## Fase 4: Ecologia Avançada (Sugestões Futuras)

-   **Predadores:** Introduzir um segundo tipo de critter que caça os outros, criando uma dinâmica de predador-presa.
-   **Reprodução Sexuada:** Implementar um sistema onde dois critters precisam se encontrar para combinar seus genes, gerando descendentes com uma mistura das características dos pais.
-   **Tempo de Vida:** Adicionar um "tempo de vida" para os critters, para que morram de velhice, forçando a necessidade de reprodução para a sobrevivência da espécie.
-   **Biomas:** Dividir o mapa em diferentes áreas (biomas) que podem favorecer diferentes genes (ex: um bioma escuro favorece critters com visão melhor).