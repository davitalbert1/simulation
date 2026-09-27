# Simulation

Coleção de simulações interativas de leis da física, biologia, ciência da computação e matemática.

## Sobre o Projeto

Este projeto contém diversas simulações visuais desenvolvidas para demonstrar conceitos científicos e matemáticos de forma interativa. As simulações são construídas utilizando a biblioteca **Dear ImGui** para renderização de interface gráfica.

## Simulações Disponíveis

### Física
- **Sistema Solar** - Simulação do sistema solar com corpos celestes
- **Ciclo de Vida Estelar** - Visualização da evolução de estrelas
- **Buraco Negro** - Simulação de propriedades de buracos negros
- **Óptica** - Simulações de reflexão e refração de luz
- **Refração e Reflexão no Vidro** - Bancada óptica 3D com janelas, lentes, lupas, garrafas e prismas móveis

### Biologia
- **Evolução** - Simulação de processos evolutivos e seleção natural

### Ciência da Computação
- **Ordenação Visual** - Visualização de algoritmos de ordenação
- **Hash** - Simula a geração e ultilização de hash em banco de dados

### Matemática
- **PI** - Simulação e cálculo de Pi

## Requisitos

- **Dear ImGui** - Biblioteca para interface gráfica
- Compilador C++ compatível com Windows
- MinGW (para build via batch files)

## ️ Compilação

Para compilar todas as simulações de uma vez:

```bash
./build_all.bat
```

Este script irá percorrer todos os diretórios e executar os arquivos `build.bat` encontrados.

## Execução

Após a compilação, execute as simulações desejadas:

### Física

#### Sistema Solar
```bash
./physics/sistem/solar.exe
```

#### Ciclo de Vida Estelar
```bash
./physics/starLifecycle/star_lifecycle.exe
```

#### Buraco Negro
```bash
./physics/blackHole/blackHole.exe
```

#### Óptica
```bash
./physics/optics/optics_sim.exe
```

#### Refração e Reflexão no Vidro
```bash
./physics/glassRefraction/glass_refraction.exe
```

### Biologia

#### Evolução
```bash
./biology/evolution/evolution_sim.exe
```

### Ciência da Computação

#### Ordenação Visual
```bash
./computerScience/visualOrdering/visual_ordering.exe
```

### Matemática

#### PI
```bash
./math/PI/pi_simulation.exe
```

## Estrutura do Projeto

```
simulation/
├── biology/
│   └── evolution/          # Simulações de biologia evolutiva
├── computerScience/
│   └── visualOrdering/     # Visualização de algoritmos de ordenação
├── math/
│   └── PI/                 # Simulações matemáticas
├── physics/
│   ├── blackHole/          # Simulação de buracos negros
│   ├── glassRefraction/    # Refração e reflexão da luz no vidro
│   ├── optics/             # Simulações de óptica
│   ├── sistem/             # Sistema solar
│   └── starLifecycle/      # Ciclo de vida estelar
├── build_all.bat           # Script de build automatizado
├── compilar.md             # Documentação de compilação
└── ideias.md               # Ideias futuras e roadmap
```

## Ideias Futuras

O projeto possui um roadmap extenso com ideias para futuras implementações:

### Física
- Pêndulo duplo caótico
- Problema gravitacional dos N corpos
- Simulação de epidemias
- Formação de galáxias
- Fluidos e dinâmica de fluidos
- Eletromagnetismo

### Biologia
- Modelo predador-presa (Lotka-Volterra)
- Propagação de doenças
- Sistemas de enxame (boids)

### Matemática
- Fractais (Mandelbrot e Julia)
- Caminhada aleatória
- Teoria dos grafos

### Astronomia
- Colisões galácticas
- Lentes gravitacionais
- Expansão do universo

Consulte o arquivo [ideias.md](ideias.md) para a lista completa de ideias planejadas.

## Documentação Adicional

- [Compilar.md](compilar.md) - Guia detalhado de compilação e execução
- [Ideias.md](ideias.md) - Lista completa de tópicos e simulações planejadas

## Licença

Este projeto está sob a licença MIT. Consulte o arquivo [LICENSE](LICENSE) para mais detalhes.

## Contribuindo

Contribuições são bem-vindas! Sinta-se à vontade para adicionar novas simulações seguindo a estrutura existente do projeto.

---

**Nota**: Este projeto foi desenvolvido com fins educacionais para demonstrar conceitos científicos através de simulações visuais interativas.