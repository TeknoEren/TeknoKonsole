# TeknoKonsole

![TeknoKonsole](assets/TeknoKonsole.png)

**TeknoKonsole**, KDE Plasma 6 ve Wayland odaklı, Qt6/QML ile geliştirilmiş fullscreen TV oyun ve uygulama launcher’ıdır. Steam oyunlarını, Linux `.desktop` uygulamalarını, Linux AppImage dosyalarını, Wine ile çalıştırılan Windows `.exe` dosyalarını, emulatorleri ve kullanıcı tanımlı komutları tek bir birleşik ana menüde gösterir.

> Proje adı uygulama içinde **TeknoKonsole**, Arch paket adı ise **tekno-konsole** olarak kullanılır.

**Mevcut kararlı sürüm: 1.0.0**

## Proje durumu

Kaynak kodu Linux üzerinde derlenebilir durumdadır. Arch Linux için `PKGBUILD` hazırdır. Windows tarafında Qt deployment, SDL2 staging ve Inno Setup installer kaynakları projeye dahildir. Windows installer binary’si GitHub kaynak koduna gömülmez; Windows build ortamında üretilerek GitHub Releases bölümüne eklenmelidir.

## Öne çıkan özellikler

| Alan | Açıklama |
|---|---|
| Birleşik ana menü | Oyunlar, emulatorler ve uygulamalar kategori seçmeden tek listede gösterilir. |
| Dikey navigasyon | Gamepad D-pad, klavye ok tuşları, mouse wheel ve touchpad ile yukarı/aşağı gezinilir. |
| Steam | AppID veya `steam://rungameid/...` ile eklenir; Steam `-silent` ile arka planda başlatılır ve Steam arayüzü açılmaz. |
| Steam artwork | AppID üzerinden Steam görselleri indirilir ve yerel kullanıcı asset dizininde önbelleğe alınır. |
| AppImage | AppImage kullanıcı dizinine kopyalanır, executable izni kontrol edilir ve emulator veya uygulama olarak çalıştırılır. |
| Linux `.desktop` | `Name`, `Exec`, `Icon` ve `Comment` alanları okunur; oyun veya emulator olarak eklenebilir. |
| Windows `.exe` | Windows’ta native, Linux’ta Wine ile çalıştırılır. Wine isteğe bağlıdır. |
| Controller yaşam döngüsü | Harici oyun veya emulator çalışırken SDL2 polling durdurulur; launcher yeniden odaklanınca controller tekrar açılır. |
| Tekil başlatma | Aynı kayıt için art arda gelen tekrar sinyalleri tek süreçle sınırlandırılır. |
| Kalıcı kayıt | Oyun, emulator, ikon, kapak ve tema verileri sistem paketinden ayrı tutulur. |

## Ekran ve navigasyon

Ana menü dikey kaydırılabilir tek bir listedir. Seçili kayıt görünür alanın dışına çıktığında liste otomatik olarak ilgili karta kayar. `Enter` veya gamepad `A` seçili kaydı başlatır; `Escape` geri döner; `Home` ana menüye gider; `F11` fullscreen geçişini yapar.

Ana menüde oyun ve emulator ekleme akışları ayrı tutulur. Steam bölümünde oyun ve `.desktop` ekleme seçenekleri bulunur. Emulator tarafında emulator ekleme, AppImage ve `.desktop` ekleme seçenekleri bulunur. Wine `.exe` dosyaları doğrudan ana menüden eklenebilir.

## Linux kurulumu

### Arch Linux paket kurulumu

Arch Linux üzerinde proje kök dizininde `makepkg` çalıştırılabilir:

```bash
makepkg -si
```

Kurulan sistem dosyalarını görmek için:

```bash
pacman -Ql tekno-konsole
```

Paket; executable dosyayı `/usr/bin/TeknoKonsole`, masaüstü girişini `/usr/share/applications/tekno-konsole.desktop`, ikonu `/usr/share/icons/hicolor/512x512/apps/` ve lisans/dokümantasyonu `/usr/share/licenses/tekno-konsole/` ile `/usr/share/doc/tekno-konsole/` altına kurar.

### Kaynak kodundan derleme

Gerekli paketler aşağıdaki komutla kurulabilir:

```bash
sudo pacman -S --needed base-devel cmake ninja pkgconf qt6-base qt6-declarative qt6-svg qt6-wayland sdl2
```

Derleme ve çalıştırma:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DBUILD_TESTING=OFF
cmake --build build
./build/bin/TeknoKonsole
```

`sudo` yalnızca sistem paketlerini kurmak için gereklidir. Kaynak kodu derlemek, paket oluşturmak ve uygulamayı çalıştırmak root gerektirmez.

## Windows dağıtımı

Windows installer kaynakları `packaging/windows/` altında bulunur. Installer per-user olarak çalışır ve varsayılan olarak `%LocalAppData%\\Programs\\TeknoKonsole` altına kurulur. Başlat menüsü kısayolu oluşturulur; masaüstü kısayolu kurulum ekranında isteğe bağlıdır. Kaldırıcı uygulama dosyalarını ve kısayolları kaldırır, kullanıcı oyunlarını ve ayarlarını silmez.

Windows build için aynı mimaride MSVC Qt6, CMake, Ninja, Inno Setup ve vcpkg SDL2 gereklidir. Aşağıdaki örnek Qt `msvc2022_64` ve vcpkg `x64-windows` kullanır:

```powershell
winget install Kitware.CMake Ninja-build.Ninja JRSoftware.InnoSetup

git clone https://github.com/microsoft/vcpkg "$env:USERPROFILE\vcpkg"
& "$env:USERPROFILE\vcpkg\bootstrap-vcpkg.bat"
& "$env:USERPROFILE\vcpkg\vcpkg.exe" install sdl2:x64-windows
```

Qt, vcpkg ve Inno Setup yollarını ayarlayıp build scriptini çalıştırın:

```powershell
$env:CMAKE_PREFIX_PATH = "C:\Qt\6.8.3\msvc2022_64"
$env:VCPKG_ROOT = "$env:USERPROFILE\vcpkg"
$env:SDL2_DLL = "$env:VCPKG_ROOT\installed\x64-windows\bin\SDL2.dll"
$env:ISCC = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"

.\packaging\windows\build-windows.ps1
```

Build tamamlandığında installer şu konumda oluşur:

```text
packaging/windows/installer/TeknoKonsole-Setup-1.0.0-x64.exe
```

`build-windows.ps1` Release CMake derlemesi yapar, `cmake --install` ile staging oluşturur, Qt’nin `windeployqt` aracını çalıştırır, SDL2.dll dosyasını ekler ve Inno Setup compiler ile installer üretir. Windows `.exe` kayıtları native çalışır; Linux tarafındaki Wine davranışı korunur.

> Linux sandbox ortamında Windows Qt toolchain’i, PowerShell ve Inno Setup Compiler bulunmadığından installer binary’si bu kaynak paketinin içinde yer almaz. Windows build tamamlandıktan sonra `.exe` dosyası GitHub Releases bölümüne yüklenmelidir.

## Kullanıcı verileri ve gizlilik

Sistem paketi kullanıcı oyunlarını, emulator ayarlarını, kişisel ikonları, kapakları, banner’ları veya temaları içermez. Linux üzerinde uygulama aşağıdaki dizinleri kullanır:

```text
~/.config/tekno-konsole/
├── games.json
├── emulators.json
├── categories.json
├── theme.json
├── settings.json
└── logo.json

~/.local/share/tekno-konsole/
├── apps/
└── assets/
    ├── logos/
    ├── icons/
    ├── covers/
    └── banners/
```

Windows üzerinde kullanıcı verileri Qt’nin kullanıcı uygulama veri konumunda tutulur. Installer ve uninstaller bu dizinleri silmez. `pacman -R tekno-konsole` Linux sistem dosyalarını kaldırır; kullanıcı verileri ancak kullanıcı tarafından ayrıca silinirse kaldırılır.

## Proje yapısı

```text
TeknoKonsole/
├── CMakeLists.txt
├── PKGBUILD
├── LICENSE
├── README.md
├── assets/
│   ├── TeknoKonsole.png
│   └── TeknoKonsole.ico
├── qml/
│   └── Main.qml
├── src/
│   ├── AppController.cpp
│   ├── AppController.h
│   └── main.cpp
└── packaging/
    ├── tekno-konsole.desktop
    └── windows/
        ├── TeknoKonsole.iss
        ├── TeknoKonsole.rc.in
        ├── build-windows.ps1
        └── make_icon.py
```

## Geliştirme notları

Qt6/QML arayüzü `qml/Main.qml` içinde, kalıcı kayıt ve işletim sistemi entegrasyonu `src/AppController.*` içinde bulunur. Steam veya üçüncü taraf emulatorler uygulamaya gömülmez; TeknoKonsole yalnızca kullanıcı tarafından seçilen kaynakları, komutları veya URI’leri başlatır.

Yeni bir özellik eklerken kullanıcı verisini sistem kurulum dosyalarından ayrı tutun. Kaynak arşivine kişisel `games.json`, `emulators.json`, asset veya tema dosyaları eklenmemelidir. Build çıktıları ve Windows installer çıktıları `.gitignore` tarafından dışarıda bırakılır.

## GitHub yayın önerisi

Kaynak kodunu GitHub repository’sine yükledikten sonra Windows installer’ı bir Windows build makinesinde oluşturun. Ardından `TeknoKonsole-Setup-1.0.0-x64.exe` dosyasını repository’nin **Releases** bölümüne binary asset olarak ekleyin. Linux kullanıcıları için kaynak arşivini ve Arch `PKGBUILD` dosyasını repository’de tutabilir, ileride Arch User Repository için ayrı bir PKGBUILD gönderimi hazırlayabilirsiniz.

GitHub’a ilk yükleme örneği:

```bash
git init
git add .
git commit -m "Initial TeknoKonsole release"
git branch -M main
git remote add origin https://github.com/KULLANICI_ADI/tekno-konsole.git
git push -u origin main
```

`KULLANICI_ADI` ve repository adresini kendi GitHub hesabınıza göre değiştirin. Kişisel kullanıcı verilerini, build klasörlerini ve gerçek installer binary’sini kaynak repository’sine eklemek yerine `.gitignore` ve GitHub Releases kullanın.

## Lisans

TeknoKonsole MIT lisansı ile dağıtılır. Ayrıntılar için [LICENSE](LICENSE) dosyasına bakın.

## References

[1]: [Qt 6 Windows deployment documentation](https://doc.qt.io/qt-6/windows-deployment.html)

[2]: [Inno Setup documentation](https://jrsoftware.org/ishelp/)

[3]: [CMake install command documentation](https://cmake.org/cmake/help/latest/command/install.html)

[4]: [Microsoft vcpkg documentation](https://learn.microsoft.com/vcpkg/)
