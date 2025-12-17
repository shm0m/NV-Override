# VM-0verride : Solution de Correction de Daltonisme (Linux / VM)

**VM-0verride** (ou *DaltonFix*) est une suite logicielle conçue pour appliquer une correction de daltonisme (Protanopie, Deutéranopie, Tritanopie) sur des systèmes Linux où l'accélération matérielle (GPU) est absente ou limitée, comme dans les machines virtuelles (VMware, VirtualBox) ou les environnements embarqués utilisant des "dumb buffers".

Ce projet contourne les limitations des shaders GPU en implémentant une correction colorimétrique :
1.  **Au niveau Kernel** : Via un module DRM (Direct Rendering Manager) qui intercepte et modifie les pixels CPU-side (arithmétique virgule fixe).
2.  **Au niveau Global (Hack)** : Via une manipulation des tables Gamma (xrandr) pour les VMs VMware récalcitrantes.
3.  **Au niveau Userspace** : Via une loupe de bureau (`dalton_cam`) pour une vérification rapide.

---

## Architecture

Le projet est composé de trois modules principaux situés dans `dalton_src/` :

| Composant | Fichier | Rôle |
| :--- | :--- | :--- |
| **Driver Kernel** | `dalton_drv.c` | Module noyau (`.ko`). Crée un pipeline d'affichage virtuel. Utilise des matrices de convolution en virgule fixe (16.16) pour transformer les couleurs (RGB -> LMS -> Correction -> RGB) directement dans la mémoire vidéo système. |
| **Dashboard** | `dalton_ui.py` | Interface graphique (Tkinter) pour piloter le driver (via `/sys/modules/...`) et activer le "Gamma Hack" pour les écrans VMware. |
| **Viewer** | `dalton_cam.py` | Outil autonome de capture d'écran et de correction en temps réel (loupe), utile si le driver noyau ne peut pas être chargé. |

---

## Installation

### Prérequis
Testé sur **Ubuntu 20.04 / 22.04 LTS** (et variantes Debian).
Le noyau Linux doit supporter `DRM` et `FBDEV`.

Installation des dépendances :
```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r) python3-tk python3-pil
```

Pour les fonctionnalités de capture d'écran (DaltonCam) :
```bash
sudo apt install gnome-screenshot scrot
```

### Compilation

1.  Accédez au dossier source :
    ```bash
    cd dalton_src
    ```

2.  Compilez le module noyau :
    ```bash
    make
    ```
    *Si tout se passe bien, un fichier `dalton_drv.ko` sera généré.*

    > **Note pour WSL2** : Si vous êtes sous WSL2 et que les sources sont sur une partition Windows (ex: `/mnt/c/...`), la compilation peut échouer à cause des espaces dans les noms de dossiers. Utilisez le script dédié :
    > ```bash
    > ./build_wsl.sh
    > ```

---

## Utilisation

### 1. Mode Natif (Linux sur PC physique ou VM compatible DRM)

C'est le mode le plus performant. Le driver intercepte l'affichage.

1.  **Charger le module** :
    ```bash
    sudo insmod dalton_drv.ko
    ```
    *Vérifiez avec `dmesg` que le message "DaltonFix Driver Initialized" apparaît.*

2.  **Lancer l'interface de contrôle** :
    ```bash
    sudo python3 dalton_ui.py
    ```
    *(Le `sudo` est obligatoire pour que l'interface puisse écrire dans `/sys/module/dalton_drv/parameters/`)*

3.  **Réglages** :
    - Sélectionnez le type de daltonisme (Protan/Deutan/Tritan).
    - Ajustez l'intensité avec le curseur.
    - La correction est appliquée directement par le noyau.

4.  **Désinstallation** (pour revenir à la normale) :
    ```bash
    sudo rmmod dalton_drv
    ```

### 2. Mode "Machine Virtuelle" (VMware / VirtualBox)

Les pilotes graphiques virtuels (vmwgfx, etc.) empêchent souvent l'injection directe de notre driver DRM sur l'affichage principal.

Si le Kernel Driver ne change pas les couleurs de votre écran principal :
1.  Lancez l'interface :
    ```bash
    sudo python3 dalton_ui.py
    ```
2.  Cochez la case **"Forcer VM (Gamma Hack)"** (ou "Application Globale").
3.  **Effet** : L'interface utilisera `xrandr --gamma` pour teinter globalement l'écran. Ce n'est pas une correction daltonienne mathématiquement parfaite (c'est une approximation par canaux), mais cela fonctionne universellement sur les VM pour aider à différencier les couleurs.

### 3. Mode Standalone (Dalton Cam)

Si vous ne pouvez pas toucher au noyau ou si vous voulez juste tester l'algorithme :

```bash
python3 dalton_cam.py
```
- Une fenêtre s'ouvre montrant une capture de votre écran (ou une zone).
- La correction algorithmique exacte (LMS Daltonization) est appliquée en Python.
- Utile pour vérifier des images statiques ou des zones précises sans modifier tout le système.

---

##  Dépannage

### "Operation not permitted" (insmod)
- Si vous avez **Secure Boot** activé dans le BIOS (ou EFI de la VM), le noyau refusera de charger notre module non signé (`dalton_drv.ko`).
- **Solution** : Désactivez le Secure Boot dans les paramètres de la VM ou signez le module (procédure avancée `mokutil`).

### L'écran ne change pas de couleur (Driver Kernel)
- Le driver `dalton_drv` crée un connecteur virtuel. Sur certaines configurations, il faut dire au système d'utiliser ce connecteur.
- Cependant, sur une session Desktop existante (GNOME/KDE), le compositeur détient déjà l'écran. C'est pourquoi le **Mode VM (Gamma Hack)** ou **Dalton Cam** sont recommandés pour une utilisation desktop standard sans reconfiguration lourde de Xorg/Wayland.

### Conflits
- Ne lancez pas le driver si un autre pilote DRM expérimental est déjà chargé.

---

*Développé par Shaima DEROUICH - Projet VM-0verride*
