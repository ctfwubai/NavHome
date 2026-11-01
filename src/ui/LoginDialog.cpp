#include "LoginDialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>

#include "app/PasswordManager.h"

LoginDialog::LoginDialog(QWidget* parent) : QDialog(parent) {
    buildUi();
}

void LoginDialog::buildUi() {
    setWindowTitle("导航之家");
    setFixedWidth(360);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);

    auto* title = new QLabel("请输入密码", this);
    title->setStyleSheet("font-size: 16px; font-weight: 600;");
    layout->addWidget(title);

    auto* desc = new QLabel("已开启密码保护，输入正确密码后进入主界面。", this);
    desc->setStyleSheet("color: #9aa3b2;");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("密码");
    layout->addWidget(m_passwordEdit);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnBox->button(QDialogButtonBox::Ok)->setText("登录");
    btnBox->button(QDialogButtonBox::Cancel)->setText("退出");
    layout->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, this, &LoginDialog::tryLogin);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &LoginDialog::tryLogin);

    m_passwordEdit->setFocus();
}

void LoginDialog::tryLogin() {
    if (PasswordManager::verify(m_passwordEdit->text())) {
        m_verified = true;
        accept();
        return;
    }
    QMessageBox::warning(this, "提示", "密码错误，请重新输入。");
    m_passwordEdit->clear();
    m_passwordEdit->setFocus();
}
