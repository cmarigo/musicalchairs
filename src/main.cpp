#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>

// Variáveis globais de controle
constexpr int TOTAL_JOGADORES = 4;
std::counting_semaphore<TOTAL_JOGADORES> sem_cadeiras(TOTAL_JOGADORES - 1); // Inicia com n-1 cadeiras
std::condition_variable cv_musica;
std::mutex mtx_musica;
std::atomic<bool> musica_pausada{false};
std::atomic<bool> jogo_em_progresso{true};

// Classe representando o Jogo das Cadeiras
class JogoCadeiras {
public:
    JogoCadeiras(int jogadores)
        : num_jogadores(jogadores), num_cadeiras(jogadores - 1) {}

    void iniciar_rodada() {
        // Reinicia a contagem de cadeiras e ajusta o semáforo
        num_cadeiras--;
        sem_cadeiras = std::counting_semaphore<TOTAL_JOGADORES>(num_cadeiras);
    }

    void parar_musica() {
        // Pausa a música e notifica os jogadores
        {
            std::lock_guard<std::mutex> lock(mtx_musica);
            musica_pausada = true;
        }
        cv_musica.notify_all();
    }

    void eliminar_jogador(int id_jogador) {
        // Imprime que um jogador foi eliminado
        std::cout << "Jogador " << id_jogador << " foi eliminado!\n";
        num_jogadores--;
    }

    void exibir_estado() {
        // Mostra o status atual do jogo
        std::cout << "Cadeiras restantes: " << num_cadeiras << "\n";
    }

private:
    int num_jogadores;
    int num_cadeiras;
};

// Classe representando um Jogador
class Jogador {
public:
    Jogador(int identificador, JogoCadeiras& jogo_ref)
        : id(identificador), jogo(jogo_ref) {}

    void tentar_sentarse() {
        // Espera pela música parar e tenta ocupar uma cadeira
        std::unique_lock<std::mutex> lock(mtx_musica);
        cv_musica.wait(lock, [] { return musica_pausada.load(); });
        if (sem_cadeiras.try_acquire()) {
            std::cout << "Jogador " << id << " conseguiu uma cadeira.\n";
        } else {
            jogo.eliminar_jogador(id);
        }
    }

    void executar() {
        while (jogo_em_progresso.load()) {
            tentar_sentarse();
            if (jogo_em_progresso.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

private:
    int id;
    JogoCadeiras& jogo;
};

// Classe para coordenar o jogo
class Coordenador {
public:
    Coordenador(JogoCadeiras& jogo_ref)
        : jogo(jogo_ref) {}

    void iniciar_jogo() {
        while (jogo_em_progresso.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Pausa simulando a música tocando
            jogo.parar_musica();
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Simula tempo até reiniciar
            jogo.iniciar_rodada();
            musica_pausada = false; // Retoma a música
            if (--rodadas <= 0) {
                jogo_em_progresso = false;
            }
        }
    }

private:
    JogoCadeiras& jogo;
    int rodadas = TOTAL_JOGADORES - 1;
};

// Função principal
int main() {
    JogoCadeiras jogo(TOTAL_JOGADORES);
    Coordenador coordenador(jogo);
    std::vector<std::thread> threads_jogadores;

    std::vector<Jogador> lista_jogadores;
    for (int i = 1; i <= TOTAL_JOGADORES; ++i) {
        lista_jogadores.emplace_back(i, jogo);
    }

    for (auto& jogador : lista_jogadores) {
        threads_jogadores.emplace_back(&Jogador::executar, &jogador);
    }

    std::thread thread_coordenador(&Coordenador::iniciar_jogo, &coordenador);

    for (auto& t : threads_jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }

    if (thread_coordenador.joinable()) {
        thread_coordenador.join();
    }

    std::cout << "O jogo terminou.\n";
    return 0;
}
