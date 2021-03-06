#include "askpassphrasedialog.h"
#include "ui_askpassphrasedialog.h"

#include "guiconstants.h"
#include "walletmodel.h"
#include "wallet.h"

#include <QMessageBox>
#include <QPushButton>
#include <QKeyEvent>

extern bool fWalletUnlockMintOnly;

AskPassphraseDialog::AskPassphraseDialog(Mode mode, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AskPassphraseDialog),
    mode(mode),
    model(0),
    fCapsLock(false)
{
    ui->setupUi(this);
    ui->passEdit1->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->passEdit2->setMaxLength(MAX_PASSPHRASE_SIZE);
    ui->passEdit3->setMaxLength(MAX_PASSPHRASE_SIZE);

    // Setup Caps Lock detection.
    ui->passEdit1->installEventFilter(this);
    ui->passEdit2->installEventFilter(this);
    ui->passEdit3->installEventFilter(this);

    switch(mode)
    {
        case Encrypt: // Ask passphrase x2
            ui->passLabel1->hide();
            ui->passEdit1->hide();
            ui->warningLabel->setText(tr("Enter the new passphrase to the wallet.<br/>Please use a passphrase of <b>10 or more random characters</b>, or <b>eight or more words</b>."));
            setWindowTitle(tr("Encrypt wallet"));
            break;
        case UnlockMinting:
            ui->mintingCheckBox->setChecked(true);
            ui->mintingCheckBox->show();
            // fallthru
        case Unlock: // Ask passphrase
            ui->warningLabel->setText(tr("This operation needs your wallet passphrase to unlock the wallet."));
            ui->passLabel2->hide();
            ui->passEdit2->hide();
            ui->passLabel3->hide();
            ui->passEdit3->hide();
            setWindowTitle(tr("Unlock wallet"));
            break;
        case EncryptOnExit:
            ui->warningLabel->setText(tr("Do you want completely encrypt your wallet? Enter a new passphrase for data encryption.<br/>Please use a passphrase of <b>10 or more characters</b>, preferrably not containing any standard/dictionary words."));
            ui->passLabel1->hide();
            ui->passEdit1->hide();
            setWindowTitle(tr("Encrypt wallet data"));
            break;
        case DecryptOnStart:
            ui->warningLabel->setText(tr("Passphrase required for startup. Enter your passphrase to decrypt the wallet data."));
            ui->passLabel2->hide();
            ui->passEdit2->hide();
            ui->passLabel3->hide();
            ui->passEdit3->hide();
            setWindowTitle(tr("Decrypt wallet data"));
            break;
        case Decrypt:   // Ask passphrase
            ui->warningLabel->setText(tr("This operation needs your wallet passphrase to decrypt the wallet."));
            ui->passLabel2->hide();
            ui->passEdit2->hide();
            ui->passLabel3->hide();
            ui->passEdit3->hide();
            setWindowTitle(tr("Decrypt wallet"));
            break;
        case ChangePass: // Ask old passphrase + new passphrase x2
            setWindowTitle(tr("Change passphrase"));
            ui->warningLabel->setText(tr("Enter the old and new passphrase to the wallet."));
            break;
    }

    textChanged();
    connect(ui->passEdit1, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
    connect(ui->passEdit2, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
    connect(ui->passEdit3, SIGNAL(textChanged(QString)), this, SLOT(textChanged()));
}

AskPassphraseDialog::~AskPassphraseDialog()
{
    // Attempt to overwrite text so that they do not linger around in memory
    ui->passEdit1->setText(QString(" ").repeated(ui->passEdit1->text().size()));
    ui->passEdit2->setText(QString(" ").repeated(ui->passEdit2->text().size()));
    ui->passEdit3->setText(QString(" ").repeated(ui->passEdit3->text().size()));
    delete ui;
}

void AskPassphraseDialog::setModel(WalletModel *model)
{
    this->model = model;
}

void AskPassphraseDialog::accept()
{
    SecureString oldpass, newpass1, newpass2;

//    if(!model)
//        return;
    oldpass.reserve(MAX_PASSPHRASE_SIZE);
    newpass1.reserve(MAX_PASSPHRASE_SIZE);
    newpass2.reserve(MAX_PASSPHRASE_SIZE);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make this input mlock()'d to begin with.
    oldpass.assign(ui->passEdit1->text().toStdString().c_str());
    newpass1.assign(ui->passEdit2->text().toStdString().c_str());
    newpass2.assign(ui->passEdit3->text().toStdString().c_str());

    switch(mode)
    {
    case Encrypt: {
        if(newpass1.empty() || newpass2.empty())
        {
            // Cannot encrypt with empty passphrase
            break;
        }
        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm wallet keys encryption"),
                 tr("Warning: If you encrypt your wallet keys and lose your passphrase, you will <b>LOSE ACCESS TO ALL OF YOUR COINS</b>!") + "<br><br>" + tr("Are you sure you wish to encrypt your wallet keys?"),
                 QMessageBox::Yes|QMessageBox::Cancel,
                 QMessageBox::Cancel);
        if(retval == QMessageBox::Yes)
        {
            if(newpass1 == newpass2)
            {
                if(model->setWalletEncrypted(true, newpass1))
                {
                    QMessageBox::warning(this, tr("Wallet encrypted"),
                                         "<qt>" +
                                         tr("CloakCoin will close now to finish the encryption process. "
                                         "Remember that encrypting your wallet keys cannot fully protect "
                                         "your coins from being stolen by malware infecting your computer.") +
                                         "<br><br><b>" +
                                         tr("IMPORTANT: Any previous backups you have made of your wallet file "
                                         "should be replaced with the newly generated, wallet file with encrypted keys. "
                                         "For security reasons, previous backups of the wallet file "
                                         "will become useless as soon as you start using the new, encrypted keys wallet.") +
                                         "</b></qt>");
                    QApplication::quit();
                }
                else
                {
                    QMessageBox::critical(this, tr("Wallet encryption failed"),
                                         tr("Wallet encryption failed due to an internal error. Your wallet was not encrypted."));
                }
                QDialog::accept(); // Success
            }
            else
            {
                QMessageBox::critical(this, tr("Wallet encryption failed"),
                                     tr("The supplied passphrases do not match."));
            }
        }
        else
        {
            QDialog::reject(); // Cancelled
        }
        } break;
    case UnlockMinting:
        fWalletUnlockMintOnly = ui->mintingCheckBox->isChecked();
    case Unlock:
        if(!model->setWalletLocked(false, oldpass))
        {
            QMessageBox::critical(this, tr("Wallet unlock failed"),
                                  tr("The passphrase entered for the wallet decryption was incorrect."));
        }
        else
        {
            QDialog::accept(); // Success
        }
        break;
    case EncryptOnExit: {
        if(newpass1.empty() || newpass2.empty()) {
            break;
        }

        QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm wallet data encryption"),
                 tr("Warning: This action will encrypt your wallet file to protect unauthorized access to your data. If you encrypt your wallet data and lose or forget your passphrase, you will <b>LOSE ACCESS TO ALL OF YOUR COINS</b>!") + "<br><br>" + tr("Press <b>YES</b> to <b>continue</b> or <b>NO</b> to <b>cancel</b>"),
                 QMessageBox::Yes|QMessageBox::No,
                 QMessageBox::No);

        if(retval == QMessageBox::Yes) {
            if(newpass1 == newpass2)
            {
                if (model->setWalletDataEncrypted(true, newpass1)){
                    QMessageBox::warning(this, tr("Wallet data encrypted"),
                                         "<qt>" +
                                         tr("CloakCoin will now exit and fully encrypt your wallet file. "
                                         "Note: Even complete wallet encryption cannot fully protect "
                                         "your coins from being stolen by malware infecting your computer, "
                                         "such as keyloggers and screen-grabbers.") +
                                         "<br><br><b>" +
                                         tr("IMPORTANT: Any previous backups you have made of your wallet file "
                                         "are deemed insecure due to exposed user/transaction data. "
                                         "Cold-storage or removal is suggested for previous backups.") +
                                         "</b></qt>");
                    //QApplication::quit();
                }
                else
                {
                    QMessageBox::critical(this, tr("Wallet data encryption failed"),
                                         tr("Complete wallet encryption <b>failed</b> due to an internal error. Your wallet file was not encrypted."));
                }
                QDialog::accept(); // Success
            }
            else
            {
                QMessageBox::critical(this, tr("Wallet data encryption failed"),
                                     tr("The supplied passphrases do not match."));
            }
        } else {
            QMessageBox::critical(this, tr("Complete wallet encryption cancelled"),
                                  tr("Complete wallet encryption was cancelled."
                                     "<br><br><b>IMPORTANT: Your wallet data was NOT fully encrypted and transaction info might be exposed "
                                     "to 3rd parties that gain access to your wallet file.</b>"));
            QDialog::reject(); // Cancelled
        }
        QApplication::quit();
        } break;

    case DecryptOnStart:
        CWallet* wallet;
        wallet = new CWallet("wallet.dat");

        if(!wallet->DecryptWalletData(oldpass))
        {
            QMessageBox::critical(this, tr("Wallet key decryption failed"),
                                  tr("The passphrase entered for the wallet key decryption was incorrect."));
            QDialog::reject(); // Cancelled
            delete wallet;
            wallet = NULL;
            QApplication::quit();
        }
        else
        {
            delete wallet;
            wallet = NULL;
            QDialog::accept(); // Success
        }
        break;

    case Decrypt:
        if(!model->setWalletEncrypted(false, oldpass))
        {
            QMessageBox::critical(this, tr("Wallet key decryption failed"),
                                  tr("The passphrase entered for the wallet key decryption was incorrect."));
        }
        else
        {
            QDialog::accept(); // Success
        }
        break;
    case ChangePass:
        if(newpass1 == newpass2)
        {
            if(model->changePassphrase(oldpass, newpass1))
            {
                QMessageBox::information(this, tr("Wallet keys encrypted"),
                                     tr("Wallet passphrase was successfully changed."));
                QDialog::accept(); // Success
            }
            else
            {
                QMessageBox::critical(this, tr("Wallet key encryption failed"),
                                     tr("The passphrase entered for the wallet decryption was incorrect."));
            }
        }
        else
        {
            QMessageBox::critical(this, tr("Wallet key encryption failed"),
                                 tr("The supplied passphrases do not match."));
        }
        break;
    }
}

void AskPassphraseDialog::textChanged()
{
    // Validate input, set Ok button to enabled when acceptable
    bool acceptable = false;
    switch(mode)
    {
    case Encrypt: // New passphrase x2
        acceptable = !ui->passEdit2->text().isEmpty() && !ui->passEdit3->text().isEmpty();
        break;
    case UnlockMinting:
        acceptable = !ui->passEdit1->text().isEmpty();
        break;
    case Unlock:
        acceptable = !ui->passEdit1->text().isEmpty();
        break;
    case EncryptOnExit:
        acceptable = !ui->passEdit2->text().isEmpty() && !ui->passEdit3->text().isEmpty() && ui->passEdit2->text() == ui->passEdit3->text();
        break;
    case DecryptOnStart:
        acceptable = !ui->passEdit1->text().isEmpty();
        break;
    case Decrypt:
        acceptable = !ui->passEdit1->text().isEmpty();
        break;
    case ChangePass: // Old passphrase x1, new passphrase x2
        acceptable = !ui->passEdit1->text().isEmpty() && !ui->passEdit2->text().isEmpty() && !ui->passEdit3->text().isEmpty();
        break;
    }
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(acceptable);
}

bool AskPassphraseDialog::event(QEvent *event)
{
    // Detect Caps Lock key press.
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_CapsLock) {
            fCapsLock = !fCapsLock;
        }
        if (fCapsLock) {
            ui->capsLabel->setText(tr("Warning: The Caps Lock key is on!"));
        } else {
            ui->capsLabel->clear();
        }
    }
    return QWidget::event(event);
}

bool AskPassphraseDialog::eventFilter(QObject *object, QEvent *event)
{
    /* Detect Caps Lock.
     * There is no good OS-independent way to check a key state in Qt, but we
     * can detect Caps Lock by checking for the following condition:
     * Shift key is down and the result is a lower case character, or
     * Shift key is not down and the result is an upper case character.
     */
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        QString str = ke->text();
        if (str.length() != 0) {
            const QChar *psz = str.unicode();
            bool fShift = (ke->modifiers() & Qt::ShiftModifier) != 0;
            if ((fShift && psz->isLower()) || (!fShift && psz->isUpper())) {
                fCapsLock = true;
                ui->capsLabel->setText(tr("Warning: The Caps Lock key is on!"));
            } else if (psz->isLetter()) {
                fCapsLock = false;
                ui->capsLabel->clear();
            }
        }
    }
    return QDialog::eventFilter(object, event);
}
