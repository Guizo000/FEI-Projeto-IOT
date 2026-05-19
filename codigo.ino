#include <Servo.h>
#include <LiquidCrystal.h>

//Vars's senha
int senha[3] = {0, 9, 5};
int senhaDigitada[3] = {0, 0, 0};
int senhaIndex = 0;
int qtdErros = 0;

//Var's LCD
LiquidCrystal lcd(A2, 12, 7, 6, 5, 4);


//Enum FSM
enum Estado 
{
  TRANCADO,
  DIGITANDO,
  VALIDANDO,
  ABERTO,
  ALERTA
};


//Enum para os tipos de alerta
enum TipoAlerta 
{
  SEM_ALERTA,
  ALERTA_ARROMBAMENTO,
  ALERTA_SENHA
};

TipoAlerta motivoAlertaAtual = SEM_ALERTA;

// --- PREVINE BUG ARDUINO ---
Estado handleValidacao();
// --------------------------


volatile Estado estadoAtual = TRANCADO; 
Estado estadoAntigo = estadoAtual; //Var auxiliar para detectar mudança de estado


//Var's servo
const int servo_pin = 10;
Servo servo;


//Var's botões
const int botao_confirma_pin = 8;
byte botao_confirma_estado_antigo;

const int botao_reset_pin = 2;

//Var's potenciometro
const int pot_pin = A3;

//Var's photoresistor
const int phtr_pin = A4;
int phtr_valor;

//Var's buzzer
const int buzzer = 11;

//Var's tempo
unsigned long tempoInicio;
unsigned long tempoAtual;
unsigned long tempoUltimoFechamento;


//----------------------------------------------------------------
//----------------------------------------------------------------
//                      SETUP E LOOP(FSM)                        |
//----------------------------------------------------------------
//----------------------------------------------------------------
void setup()
{
  //Iniciando monitor serial
  Serial.begin(9600);
  
  //Iniciando servo
  servo.attach(servo_pin);
  servo.write(0);
  
  //Iniciando LCD
  lcd.begin(16, 2);  
  
  //Iniciando botões
  pinMode(botao_confirma_pin, INPUT_PULLUP);
  botao_confirma_estado_antigo = digitalRead(botao_confirma_pin);
  
  pinMode(botao_reset_pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(botao_reset_pin), reset, FALLING);

  //Inicializando tempos
  tempoInicio = millis();
  tempoUltimoFechamento = 2000;

  //Fazendo setup estado inicial TRANCADO
  printLcd(4, 0, "TRANCADO"); 
  printLcd(4, 1, "Erros:" + String(qtdErros)); 
  servo.write(0); //Posiciona o servo na posição "fechado"
}

void loop()
{
  tempoAtual = millis();
  phtr_valor = analogRead(phtr_pin);
   
  if(phtr_valor < 160 && estadoAtual != ABERTO && tempoAtual - tempoUltimoFechamento > 2000)
  {
    estadoAtual = ALERTA;
    motivoAlertaAtual = ALERTA_ARROMBAMENTO;
    senhaIndex = 0;
    qtdErros = 0; 
  }

  switch(estadoAtual) 
  {
    //ESTADO TRANCADO
    case TRANCADO:
      //Setup estado
      if(estadoAntigo != estadoAtual)
      {
        setupTrancado();
        estadoAntigo = estadoAtual;       
      }
  
      //Corpo Estado
      if(edgeDetector(botao_confirma_pin, botao_confirma_estado_antigo, true, false))
      {
        estadoAtual = DIGITANDO;
      }
      break;
    
    //ESTADO DIGITANDO
    case DIGITANDO:
      //Setup estado
      if(estadoAntigo != estadoAtual)
      {
        setupDigitando();
        estadoAntigo = estadoAtual;
      }
       
      //Corpo Estado
      if(handleSenha()) 
      {
        estadoAtual = VALIDANDO;
      }
      break;
    
    //ESTADO VALIDANDO
    case VALIDANDO:
      //Setup estado
      if (estadoAntigo != estadoAtual)
      {
        setupValidando(); 
        estadoAntigo = estadoAtual;
      }
      
      //Corpo Estado
      //Verifica tempo decorrido desde mudança de estado
      if(tempoAtual - tempoInicio >= 500)
      {
        Estado proximoEstado = handleValidacao();
        if(proximoEstado == TRANCADO) 
        {
          estadoAtual = TRANCADO;
        }
        else if(proximoEstado == ABERTO)
        {
          estadoAtual = ABERTO;
        }
        else if(proximoEstado == ALERTA)
        {
          estadoAtual = ALERTA; 
        }
      }
      break;
    
    //ESTADO ABERTO
    case ABERTO:
      //Setup estado
      if(estadoAntigo != estadoAtual)
      {
        setupAberto();
        estadoAntigo = estadoAtual;
      }
      
    
      //Corpo Estado
      if(tempoAtual - tempoInicio >= 5000) 
      {
        //Seta tempo inicio para pausar verificação
        //do phtr enquanto cofre estiver fechando
        tempoInicio = tempoAtual;
        estadoAtual = TRANCADO;
      }
      else
      {
        printLcd(3, 1, "Aberto:"); 	
        tempoUltimoFechamento = tempoAtual;
        //Counter ate fechamento
        lcd.print(5 - (tempoAtual-tempoInicio) / 1000);
      }
      break;

    //ESTADO ALERTA
    case ALERTA:
      //Setup estado
      if(estadoAntigo != estadoAtual)
      {
        setupAlerta();
        estadoAntigo = estadoAtual; 
      }
    
      //Corpo Estado
      if(motivoAlertaAtual == ALERTA_SENHA) 
      {
        //Timer ate liberar bloqueio
        if (tempoAtual - tempoInicio < 5000)
        {
          printLcd(3, 1, "Bloqueado:"); 	
          lcd.print(5 - (tempoAtual-tempoInicio) / 1000);
        }
        else
        {
          estadoAtual = TRANCADO;
          motivoAlertaAtual = SEM_ALERTA;
        }        
      }
      else if(motivoAlertaAtual == ALERTA_ARROMBAMENTO)  
      {
        printLcd(3, 1, "Violacao!!");  
        tone(buzzer, 392);
      }
      break;
  }
  	
}


//----------------------------------------------------------------
//----------------------------------------------------------------
//                      FSM FUNCTIONS                            |
//----------------------------------------------------------------
//----------------------------------------------------------------
//TRANCADO
void setupTrancado()
{
  lcd.clear();
  printLcd(4, 0, "TRANCADO");
  printLcd(4, 1, "Erros:");  
  lcd.print(qtdErros);
  //Posiciona o servo na posição "fechado"
  servo.write(0);
}


//DIGITANDO
void setupDigitando()
{
  lcd.clear();
  printLcd(0, 0, "Insira Sua Senha");
  printLcd(6, 1, "___");
}

bool handleSenha()
{
  //Printa digito atual na tela
  printLcd(senhaIndex+6, 1, String(map(analogRead(pot_pin), 0, 1023, 0, 9)));
   
  //Verifica se o botao foi apertado para confirmar digito de senha
  if(edgeDetector(botao_confirma_pin, botao_confirma_estado_antigo, true, false))
  {
    //Converte escala do potenciometro (0 a 1023) para escala de 0 a 9
    int leituraPot = analogRead(pot_pin);
    senhaDigitada[senhaIndex] = map(leituraPot, 0, 1023, 0, 9);
    //Avança var's para próximo digito
    senhaIndex++;
    //Caso tenha sido o ultimo digito, reseta o index e retorna true (senha inserida)
    if(senhaIndex == 3)
    {
      senhaIndex = 0;
      return true;
    }
  } 
  
  return false; 
}


//VALIDANDO
void setupValidando()
{
  lcd.clear();
  printLcd(3, 0, "VALIDANDO");
  tempoInicio = tempoAtual;
}

Estado handleValidacao()
{
  for(int i = 0; i < 3; i++)
  {
    if(senha[i] == senhaDigitada[i])
    {
      continue; //Se digito for igual pula pra proxima iteração
    }

    //Se digito for diferente, aumenta qtd erros 
    qtdErros++;

    //Se tiver errado 3 vezes, retorna estado ALERTA, senão retorna estado TRANCADO
    if(qtdErros == 3)
    {
      qtdErros = 0;
      motivoAlertaAtual = ALERTA_SENHA;
      return ALERTA;
    }
    return TRANCADO; 
  }
  //Caso a senha tenha sido digitado de forma correta, retorna estado ABERTO
  return ABERTO;
}


//ABERTO
void setupAberto()
{
  lcd.clear();
  printLcd(5, 0, "ABERTO");
  //Posiciona o servo na posição "aberto"
  servo.write(90);
  tempoInicio = tempoAtual;
  qtdErros = 0;
}


//ALERTA
void setupAlerta()
{
  lcd.clear();
  printLcd(5, 0, "ALERTA");   
  tempoInicio = tempoAtual;
}


//----------------------------------------------------------------
//----------------------------------------------------------------
//                      UTILITY FUNCTIONS                        |
//----------------------------------------------------------------
//----------------------------------------------------------------
//Seta o cursor e printa o texto 
void printLcd(int posX, int posY, String text)
{
  lcd.setCursor(posX, posY);
  lcd.print(text);
}

//Detecta borda de subida ou descida do botão passada no argumento
bool edgeDetector(int botao_pin, byte &estado_antigo, bool down, bool up)
{
  byte estado_atual = digitalRead(botao_pin);
  
  //Verifica se houve mudança de estado desde a ultima leitura
  if(estado_atual != estado_antigo)
  {
    // HIGH -> LOW (pressionou)
    if(estado_antigo == HIGH)
    {
      estado_antigo = estado_atual;
      return down;
    }

    // LOW -> HIGH (soltou)
    if(estado_antigo == LOW)
    {
      estado_antigo = estado_atual;
      return up;
    }
  }
  
  return false; //Caso estado do botao antigo = atual
}


//----------------------------------------------------------------
//----------------------------------------------------------------
//                      INTERRUPT FUNCTIONS                      |
//----------------------------------------------------------------
//----------------------------------------------------------------
void reset()
{
  if(estadoAtual != ALERTA)
  {
    estadoAtual = TRANCADO;
    senhaIndex = 0;
    motivoAlertaAtual = SEM_ALERTA;
  }
}
