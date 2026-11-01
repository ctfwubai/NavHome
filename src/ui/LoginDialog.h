#pragma once

#include <QDialog>

class QLineEdit;

// 密码登录对话框（对应需求 3.7.1）
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);

    // 是否通过验证
    bool verified() const { return m_verified; }

private:
    void buildUi();
    void tryLogin();

    QLineEdit* m_passwordEdit = nullptr;
    bool m_verified = false;
};
