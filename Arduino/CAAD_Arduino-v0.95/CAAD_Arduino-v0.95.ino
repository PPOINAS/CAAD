#include <SPI.h>  //Inclut la bibliothèque SPI qui permet de communiquer avec des appareils par un bus Serial Peripheral Interface
#include <MFRC522.h>  //Inclut la bibliothèque MFRC522 pour manipuler le capteur RFID RC522
#include <Servo.h>  //Inclut la bibliothèque Servo pour manipuler le servomoteur
#include <SoftwareSerial.h> //Inclut la bibliothèque SoftwareSerial pour manipuler le module Bluetooth HC-05
#include <Stepper.h>  //Inclut la bibliothèque Stepper pour controler le moteur pas-à-pas
#include <Wire.h> //Inclut la bibliothèque Wire pour communiquer par le protocle I2C avec l'horloge
#include <RTClib.h> //Inclut la bibliothèque RTClib qui permet de controler l'horloge DS1307
 
#define SS_PIN 53
#define RST_PIN 3
#define Nombre_chats_max 5
#define CapteurInfra 23

String UID_Carte;
String UID_Autorise[Nombre_chats_max] = {""};   //Liste de chaines de caractères contenant les noms des chats
String NomsChats_Autorise[Nombre_chats_max] = {""};  //Liste de chaines de caractères contenant les noms des chats
bool Chat_Interieur[Nombre_chats_max] = {};  //Liste de variables booléennes, true si le chat est à l'interieur, false s'il est à l'extérieur
int NbChats_enregistre = 0; //Nombre de chats enregistrés dans la chatière
String donnees_in;  //Données sous formes d'une chaine de caractères reçues par Bluetooth
const int StepsPerRevolution = 2048;  //Nombre de pas par révolution du (moteur pas-à-pas + réducteur)  |ici un 28BYJ-48|
String Heuredistrib = "19:00";  //Heure de distribution définie
String Temps_actuel; //Chaîne de caractères pour stocker la valeur du temps réel
bool Verrouillage = false;  //true si chatière verrouillée, false si chatière déverrouillée
int Compteur_Nourriture = 0;  //Variable qui va s'incrémenter jusqu'à 7 avant d'avertir l'utilisateur du manque de nourriture
bool Alerte = false;

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Creation une instance pour notre lecteur RFID MFRC522
Servo Servomoteur;  //Creation une instance pour contrôler le servomoteur
SoftwareSerial ModuleBT(10, 11); //Creation une instance pour contrôler le module HC-05
Stepper StepperMotor(StepsPerRevolution, 4, 5, 6, 7);
RTC_DS1307 RTC;


void setup() {
  Serial.begin(9600); //Initialisation de la communication avec le PC à 9600 Bauds
  SPI.begin(); //Initialisation de la communication par bus SPI
  mfrc522.PCD_Init(); //Initialisation du lecteur RFID MFRC522
  pinMode(CapteurInfra, INPUT); //Initialisation du capteur infrarouge FC-51 à l'interieur
  Servomoteur.attach(8); //Servomoteur branché sur le pin 8
  ModuleBT.begin(9600); //Initialisation de la communication avec le HC-05 à 9600 Bauds
  StepperMotor.setSpeed(3);
  Wire.begin();
  RTC.begin();
  RTC.adjust(DateTime(F(__DATE__), F(__TIME__))); //Mise à jour de l'horloge par rapport à l'ordinateur
  Servomoteur.write(0); //Met la chatière en position fermée
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max); //Maximiser la distance de lecture
}


void loop() {
  DetectionSortie();
  Reception_BT();
  Distribution();
  
  if ((mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) && Verrouillage == false){ //Si une carte est présente et que la carte a été lue    !! || plutôt que && si BUG
    UID_Carte = Recuperation_UID();
    for (int i = 0; i <= Nombre_chats_max-1; i++) {  //Boucle pour comparer l'UID scannée aux UID autorisées
      if (UID_Carte == UID_Autorise[i]){  //Si l'UID scannée corespond à un chat enregistré dans la chatière
        OuvertureChatiere();
        Chat_Interieur[i] = true; //Le chat vient d'entrer
      }
    }
    UID_Carte = ""; //Réinitialse la variable contenant l'UID (surement inutile de le faire)
    delay (100); //Peut détecter un nouveau chat 100ms après que la chatière se soit fermée
  }
}




String Recuperation_UID() {
  String UID = "";    //Créer une chaine de caractères servant à stocker l'UID
  for (byte i = 0; i < mfrc522.uid.size; i++) { //Boucle servant à recréer l'UID complète à partir de fragment hexadécimaux
    UID += String(mfrc522.uid.uidByte[i], HEX);
  }
  UID.toUpperCase();  //Mettre l'UID en majuscule
  return(UID);
}


void OuvertureChatiere() {
  //Ouvre la chatière 
  for (int position = 0; position <= 90; position += 5) { //Régler l'incrément et le delay si trop rapide (ici durée d'ouverture de 285ms)
    Servomoteur.write(position);
    delay(15);
  }
  delay(8000);  //Attend 8 secondes que le chat soit passé
  //Écrit au servomoteur une position de plus en plus faible pour fermer progresssivement la chatière
  for (int position = 90; position >= 0; position--) {  //Régler l'incrément et le delay si trop rapide/trop lent (ici durée de fermeture de 3,19s)
    Servomoteur.write(position);
    delay(35);
  }
}


void Distribution() {
  DateTime now = RTC.now(); //Récupère l'heure sur l'horloge
  String Heures = String(now.hour()); //Écrit l'heure sous forme de chaîne de caractères
  String Minutes = String(now.minute());  //Écrit les minutes sous forme de chaîne de caractères
  Temps_actuel = String(Heures + ":" + Minutes);  //Rassemble les chaines de caractères
  if (Temps_actuel == Heuredistrib) {
    for (int i = 0; i <= Nombre_chats_max-1; i++) {  //Boucle pour comparer l'UID scannée aux UID autorisées
      if (Chat_Interieur[i] == true){  //Si le chat est à l'intérieur
        StepperMotor.step(StepsPerRevolution/8); //Le moteur pas-à-pas fait un huitième de tour
        break;
      }
    }
    Compteur_Nourriture += 1;
    if (Compteur_Nourriture == 7){
      Alerte = true;
    }
    delay(70000); //Attent une 1min 10s pour que l'heure puisse avoir changée (ne peux pas ouvrir la chatière ni communiquer avec le smartphone pendant ce temps là)
  }
}


void DetectionSortie() {
  if (digitalRead(CapteurInfra) == LOW) { //Si le chat est devant le capteur
    Servomoteur.write(90);  //Ouvre la chatière
    //Verifie quel chat sort ou s'il s'agit d'une erreur
    DateTime now = RTC.now(); //Récupère l'heure sur l'horloge
    int Minutes = int(now.minute());  //Écrit les minutes sous forme d'un nombre
    if ((Minutes == 58) or (Minutes == 59)){  //Condition qui empèche la chatière de rester ouverte 1h si présence d'un faux-positif (ouverte seulement 4 min max)
      Minutes = 0;
    }
    while (!(mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())) {  //Ne fait rien tant qu'une carte n'a pas été détecté (tant que le chat n'est pas sorti)
      DateTime now = RTC.now();
      if (int(now.minute()) == Minutes + 1){  //Si ça fait 2 min quelle est ouverte et que le chat n'est pas détecté
        goto FauxPositif; //On considère un faux-positif
      }
    } 
    //Détecte la carte du chat afin de confirmer quel chat est sorti
    UID_Carte = Recuperation_UID();
    for (int i = 0; i <= Nombre_chats_max-1; i++) {  //Boucle pour comparer l'UID scannée aux UID autorisées
      if (UID_Carte == UID_Autorise[i]){ 
      Chat_Interieur[i] = false;  //Le chat vient de sortir
      }
    }
    delay(3000);  //Attend 3 secondes pour être sur que le chat soit passé (même après que sa puce ai été détéctée (donc à l'exterieur))
    FauxPositif:
    for (int position = 90; position >= 0; position--) {  //Écrit au servomoteur une position de plus en plus faible pour fermer progresssivement la chatière
      Servomoteur.write(position);
      delay(35);
    }
  }
}


void Reception_BT() {
  while (ModuleBT.available()){
    donnees_in = ModuleBT.readString(); //Stocker les données entrantes dans une variable
    char PremierCaractere = donnees_in.charAt(0); //On extrait le premier caractère de chaque message
    switch (PremierCaractere) {
      case 'A' :
        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()){
          UID_Carte = Recuperation_UID();
          UID_Autorise[NbChats_enregistre] = UID_Carte;
          NomsChats_Autorise[NbChats_enregistre] = donnees_in.substring(1); //Enregistre le nom du chat dans une liste
          NbChats_enregistre = NbChats_enregistre + 1;  //Un chat de plus a été enregistré
        }
        break;
      case 'S' :
          if (donnees_in.substring(1) == "Ouvert"){
            Verrouillage = false;
          }
          else if (donnees_in.substring(1) == "Ferme"){
            Verrouillage = true;
          }
        break;
      case 'R' :
        int RangChatSupp = (donnees_in.substring(1)).toInt();
        NbChats_enregistre = NbChats_enregistre - 1;
        for(int i = RangChatSupp; i < Nombre_chats_max-1; i++){
          UID_Autorise[i] = UID_Autorise[i+1];
          NomsChats_Autorise[i] = NomsChats_Autorise[i+1];
          Chat_Interieur[i] = Chat_Interieur[i+1];
        }
        UID_Autorise[Nombre_chats_max-1] = "";
        NomsChats_Autorise[Nombre_chats_max-1] = "";
        Chat_Interieur[Nombre_chats_max-1] = "";
        break;
      case 'D' :
        Heuredistrib = donnees_in.substring(1);
        break;
      case 'I' :
        //Données envoyée sous la forme : {$StatutVerrouillage$Heuredistrib$Alerte$NbChatsEnregistrés$NomchatN°1£StatutChat_InterieurN°1$NomchatN°2£StatutChat_InterieurN°2.....}
        String donnees_out = Verrouillage + "$" + Heuredistrib + "$" + String(Alerte) + "$" + String(NbChats_enregistre)+ "$"; //Création d'une chaine de caractères contenat les données à envoyer par Bluetooth
        for (int i = 0; i < Nombre_chats_max-1; i++){
          donnees_out += NomsChats_Autorise[i] + "£" + String(Chat_Interieur[i]);
        }
        ModuleBT.println(donnees_out);
        break;
      default:
        break;
    }
  }
}
/* !!! Les espaces et les crochets ne sont là que pour la lecture et ne doivent pas appartenir à la chaîne de caractères reçue !!!
 * Ajouter un chat : {A NomChat}
 * Déverrouiller chatière : {S Ouvert}   Verrouiller chatière : {S Ferme}
 * Supprimer un chat : {R NuméroduChat}
 * Changer l'heure de distribution de nourriture {D HeuredeDistrib}
 * Envoyer des infos au téléphone {I}
 */
