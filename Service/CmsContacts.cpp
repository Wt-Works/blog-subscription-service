/**
 * @file
 * @author  Mamadou Babaei <info@babaei.net>
 * @version 0.1.0
 *
 * @section LICENSE
 *
 * (The MIT License)
 *
 * Copyright (c) 2016 - 2019 Mamadou Babaei
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @section DESCRIPTION
 *
 * Manage website contacts.
 */


#include <boost/algorithm/string.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/format.hpp>
#include <pqxx/pqxx>
#include <Wt/WApplication>
#include <Wt/WCheckBox>
#include <Wt/WInPlaceEdit>
#include <Wt/WLengthValidator>
#include <Wt/WLineEdit>
#include <Wt/WMessageBox>
#include <Wt/WPushButton>
#include <Wt/WRegExpValidator>
#include <Wt/WSignalMapper>
#include <Wt/WString>
#include <Wt/WTable>
#include <Wt/WTemplate>
#include <Wt/WText>
#include <Wt/WWidget>
#include <CoreLib/Crypto.hpp>
#include <CoreLib/Database.hpp>
#include <CoreLib/FileSystem.hpp>
#include <CoreLib/Log.hpp>
#include <CoreLib/make_unique.hpp>
#include "CgiEnv.hpp"
#include "CgiRoot.hpp"
#include "CmsContacts.hpp"
#include "Div.hpp"
#include "Pool.hpp"

using namespace std;
using namespace boost;
using namespace Wt;
using namespace pqxx;
using namespace CoreLib;
using namespace Service;

struct CmsContacts::Impl : public Wt::WObject
{
public:
    Wt::WLineEdit *RecipientEnLineEdit;
    Wt::WLineEdit *RecipientFaLineEdit;
    Wt::WLineEdit *EmailLineEdit;
    Wt::WCheckBox *IsDefaultRecipientCheckBox;
    Wt::WText *EditContactsMessageArea;
    Wt::WContainerWidget *ContactsTableContainer;

    std::unique_ptr<Wt::WMessageBox> EraseMessageBox;

private:
    CmsContacts *m_parent;

public:
    explicit Impl(CmsContacts *parent);
    ~Impl();

public:
    void OnAddContactFormSubmitted();
    void OnCellSaveButtonPressed(Wt::WInPlaceEdit *inPlaceEdit);
    void OnSetDefaultCheckBoxStateChanged(Wt::WCheckBox *checkbox);
    void OnEraseButtonPressed(Wt::WPushButton *button);
    void OnEraseDialogClosed(Wt::StandardButton button);

public:
    void FillContactsDataTable();

private:
    Wt::WInPlaceEdit *GetContactsCell(const std::string &cellValue,
                                      const std::string &dbKey,
                                      const std::string &dbField,
                                      Wt::WSignalMapper<Wt::WInPlaceEdit *> *signalMapper);
};

CmsContacts::CmsContacts()
    : Page(),
    m_pimpl(make_unique<CmsContacts::Impl>(this))
{
    this->clear();
    this->setId("CmsContactsPage");
    this->addWidget(this->Layout());
}

CmsContacts::~CmsContacts() = default;

WWidget *CmsContacts::Layout()
{
    Div *container = new Div("CmsContacts", "container-fluid");

    try {
        CgiRoot *cgiRoot = static_cast<CgiRoot *>(WApplication::instance());
        CgiEnv *cgiEnv = cgiRoot->GetCgiEnvInstance();

        string htmlData;
        string file;
        if (cgiEnv->GetInformation().Client.Language.Code
                == CgiEnv::InformationRecord::ClientRecord::LanguageCode::Fa) {
            file = "../templates/cms-contacts-fa.wtml";
        } else {
            file = "../templates/cms-contacts.wtml";
        }

        if (CoreLib::FileSystem::Read(file, htmlData)) {
            /// Fill the template
            WTemplate *tmpl = new WTemplate(container);
            tmpl->setTemplateText(WString::fromUTF8(htmlData), TextFormat::XHTMLUnsafeText);

            m_pimpl->RecipientEnLineEdit = new WLineEdit();
            m_pimpl->RecipientEnLineEdit->setPlaceholderText(tr("cms-contacts-recipient-name-en-placeholder"));
            WLengthValidator *recipientEnValidator = new WLengthValidator(Pool::Storage().MinEmailRecipientNameLength(),
                                                                          Pool::Storage().MaxEmailRecipientNameLength());
            recipientEnValidator->setMandatory(true);
            m_pimpl->RecipientEnLineEdit->setValidator(recipientEnValidator);

            m_pimpl->RecipientFaLineEdit = new WLineEdit();
            m_pimpl->RecipientFaLineEdit->setPlaceholderText(tr("cms-contacts-recipient-name-fa-placeholder"));
            WLengthValidator *recipientFaValidator = new WLengthValidator(Pool::Storage().MinEmailRecipientNameLength(),
                                                                          Pool::Storage().MaxEmailRecipientNameLength());
            recipientFaValidator->setMandatory(true);
            m_pimpl->RecipientFaLineEdit->setValidator(recipientFaValidator);

            m_pimpl->EmailLineEdit = new WLineEdit();
            m_pimpl->EmailLineEdit->setPlaceholderText(tr("cms-contacts-email-address-placeholder"));
            WRegExpValidator *emailValidator = new WRegExpValidator(Pool::Storage().RegexEmail());
            emailValidator->setMandatory(true);
            m_pimpl->EmailLineEdit->setValidator(emailValidator);

            m_pimpl->IsDefaultRecipientCheckBox = new WCheckBox();
            m_pimpl->IsDefaultRecipientCheckBox->setStyleClass("checkbox");

            WPushButton *addPushButton = new WPushButton(tr("cms-contacts-add"));
            addPushButton ->setStyleClass("btn btn-default");

            m_pimpl->EditContactsMessageArea = new WText();
            HtmlInfo(tr("cms-contacts-edit-hint").value(), m_pimpl->EditContactsMessageArea);

            m_pimpl->ContactsTableContainer = new Div("ContactsTableContainer", "contacts-table-container");
            m_pimpl->FillContactsDataTable();

            tmpl->bindString("recipient-en-input-id", m_pimpl->RecipientEnLineEdit->id());
            tmpl->bindString("recipient-fa-input-id", m_pimpl->RecipientFaLineEdit->id());
            tmpl->bindString("email-input-id", m_pimpl->EmailLineEdit->id());

            tmpl->bindWidget("edit-contacts-title", new WText(tr("cms-contacts-page-title")));

            tmpl->bindWidget("recipient-en-label-text", new WText(tr("cms-contacts-recipient-name-en")));
            tmpl->bindWidget("recipient-fa-label-text", new WText(tr("cms-contacts-recipient-name-fa")));
            tmpl->bindWidget("email-label-text", new WText(tr("cms-contacts-email-address")));
            tmpl->bindWidget("is-default-recipient-label-text", new WText(tr("cms-contacts-is-default-recipient")));

            tmpl->bindWidget("recipient-en-input", m_pimpl->RecipientEnLineEdit);
            tmpl->bindWidget("recipient-fa-input", m_pimpl->RecipientFaLineEdit);
            tmpl->bindWidget("email-input", m_pimpl->EmailLineEdit);
            tmpl->bindWidget("is-default-recipient-input", m_pimpl->IsDefaultRecipientCheckBox);

            tmpl->bindWidget("add-button", addPushButton);

            tmpl->bindWidget("edit-contacts-message-area", m_pimpl->EditContactsMessageArea);

            tmpl->bindWidget("contacts-table", m_pimpl->ContactsTableContainer);

            m_pimpl->RecipientEnLineEdit->enterPressed().connect(m_pimpl.get(), &CmsContacts::Impl::OnAddContactFormSubmitted);
            m_pimpl->RecipientFaLineEdit->enterPressed().connect(m_pimpl.get(), &CmsContacts::Impl::OnAddContactFormSubmitted);
            m_pimpl->EmailLineEdit->enterPressed().connect(m_pimpl.get(), &CmsContacts::Impl::OnAddContactFormSubmitted);
            addPushButton->clicked().connect(m_pimpl.get(), &CmsContacts::Impl::OnAddContactFormSubmitted);

            m_pimpl->RecipientEnLineEdit->setFocus();
        }
    }

    catch (const boost::exception &ex) {
        LOG_ERROR(boost::diagnostic_information(ex));
    }

    catch (const std::exception &ex) {
        LOG_ERROR(ex.what());
    }

    catch (...) {
        LOG_ERROR(UNKNOWN_ERROR);
    }

    return container;
}

CmsContacts::Impl::Impl(CmsContacts *parent)
    : m_parent(parent)
{

}

CmsContacts::Impl::~Impl() = default;

void CmsContacts::Impl::OnAddContactFormSubmitted()
{
    if (!m_parent->Validate(RecipientEnLineEdit)
            || !m_parent->Validate(RecipientFaLineEdit)
            || !m_parent->Validate(EmailLineEdit)) {
        return;
    }

    m_parent->HtmlInfo(L"", EditContactsMessageArea);

    CgiRoot *cgiRoot = static_cast<CgiRoot *>(WApplication::instance());
    CgiEnv *cgiEnv = cgiRoot->GetCgiEnvInstance();

    try {
        string recipient(RecipientEnLineEdit->text().trim().toUTF8());

        auto conn = Pool::Database().Connection();
        conn->activate();
        pqxx::work txn(*conn.get());

        string query((format("SELECT recipient FROM \"%1%\""
                             " WHERE recipient = %2%;")
                      % Pool::Database().GetTableName("CONTACTS")
                      % txn.quote(recipient)).str());
        LOG_INFO("Running query...", query, cgiEnv->GetInformation().ToJson());

        result r = txn.exec(query);

        if (!r.empty()) {
            m_parent->HtmlError(tr("cms-contacts-duplicate-error"), EditContactsMessageArea);
            RecipientEnLineEdit->setFocus();
            return;
        }

        string recipient_fa(RecipientFaLineEdit->text().trim().toUTF8());
        string email(EmailLineEdit->text().trim().toUTF8());

        if (IsDefaultRecipientCheckBox->isChecked()) {
            Pool::Database().Update("CONTACTS",
                                     "1",
                                     "1",
                                     "is_default=?",
                                     { "FALSE" });
        }

        Pool::Database().Insert("CONTACTS",
                                 "recipient, recipient_fa, address, is_default",
                                 { recipient, recipient_fa, email,
                                   lexical_cast<string>(IsDefaultRecipientCheckBox->isChecked()) });

        RecipientEnLineEdit->setText("");
        RecipientFaLineEdit->setText("");
        EmailLineEdit->setText("");
        RecipientEnLineEdit->setFocus();

        FillContactsDataTable();
    }

    catch (const pqxx::sql_error &ex) {
        LOG_ERROR(ex.what(), ex.query(), cgiEnv->GetInformation().ToJson());
    }

    catch (const boost::exception &ex) {
        LOG_ERROR(boost::diagnostic_information(ex), cgiEnv->GetInformation().ToJson());
    }

    catch (const std::exception &ex) {
        LOG_ERROR(ex.what(), cgiEnv->GetInformation().ToJson());
    }

    catch (...) {
        LOG_ERROR(UNKNOWN_ERROR, cgiEnv->GetInformation().ToJson());
    }
}

void CmsContacts::Impl::OnCellSaveButtonPressed(Wt::WInPlaceEdit *inPlaceEdit)
{
    if (!m_parent->Validate(inPlaceEdit->lineEdit())) {
        FillContactsDataTable();
        return;
    }

    CgiRoot *cgiRoot = static_cast<CgiRoot *>(WApplication::instance());
    CgiEnv *cgiEnv = cgiRoot->GetCgiEnvInstance();

    try {
        string recipient(inPlaceEdit->attributeValue("db-key").toUTF8());

        auto conn = Pool::Database().Connection();
        conn->activate();
        pqxx::work txn(*conn.get());

        string query((format("SELECT recipient FROM \"%1%\""
                             " WHERE recipient = %2%;")
                      % Pool::Database().GetTableName("CONTACTS")
                      % txn.quote(recipient)).str());
        LOG_INFO("Running query...", query, cgiEnv->GetInformation().ToJson());

        result r = txn.exec(query);

        if (r.empty()) {
            m_parent->HtmlError(tr("cms-contacts-not-found-error"), EditContactsMessageArea);
            return;
        }

        string field(inPlaceEdit->attributeValue("db-field").toUTF8());
        string value(inPlaceEdit->text().trim().toUTF8());

        if (field == "recipient" && recipient != value) {
            query.assign((format("SELECT recipient FROM \"%1%\""
                                 " WHERE recipient = %2%;")
                          % Pool::Database().GetTableName("CONTACTS")
                          % txn.quote(value)).str());
            LOG_INFO("Running query...", query, cgiEnv->GetInformation().ToJson());

            r = txn.exec(query);

            if (!r.empty()) {
                m_parent->HtmlError(tr("cms-contacts-duplicate-error"), EditContactsMessageArea);
                FillContactsDataTable();
                return;
            }
        }

        if (field == "recipient_fa") {
            query.assign((format("SELECT recipient FROM \"%1%\""
                                 " WHERE recipient_fa = %2%;")
                          % Pool::Database().GetTableName("CONTACTS")
                          % txn.quote(value)).str());
            LOG_INFO("Running query...", query, cgiEnv->GetInformation().ToJson());

            r = txn.exec(query);

            if (!r.empty()) {
                const pqxx::row row(r[0]);
                const string recipientKey(row["recipient"].c_str());

                if (recipient != recipientKey) {
                    m_parent->HtmlError(tr("cms-contacts-duplicate-error"), EditContactsMessageArea);
                    FillContactsDataTable();
                    return;
                }
            }
        }

        Pool::Database().Update("CONTACTS",
                                 "recipient",
                                 recipient,
                                 (format("%1%=?") % field ).str(),
                                 { value });

        m_parent->HtmlInfo(L"", EditContactsMessageArea);

        FillContactsDataTable();
    }

    catch (const pqxx::sql_error &ex) {
        LOG_ERROR(ex.what(), ex.query(), cgiEnv->GetInformation().ToJson());
    }

    catch (const boost::exception &ex) {
        LOG_ERROR(boost::diagnostic_information(ex), cgiEnv->GetInformation().ToJson());
    }

    catch (const std::exception &ex) {
        LOG_ERROR(ex.what(), cgiEnv->GetInformation().ToJson());
    }

    catch (...) {
        LOG_ERROR(UNKNOWN_ERROR, cgiEnv->GetInformation().ToJson());
    }
}

void CmsContacts::Impl::OnSetDefaultCheckBoxStateChanged(Wt::WCheckBox *checkbox)
{
    CgiRoot *cgiRoot = static_cast<CgiRoot *>(WApplication::instance());
    CgiEnv *cgiEnv = cgiRoot->GetCgiEnvInstance();

    try {
        string recipient(checkbox->attributeValue("db-key").toUTF8());

        auto conn = Pool::Database().Connection();
        conn->activate();
        pqxx::work txn(*conn.get());

        string query((format("SELECT recipient FROM \"%1%\""
                             " WHERE recipient = %2%;")
                      % Pool::Database().GetTableName("CONTACTS")
                      % txn.quote(recipient)).str());
        LOG_INFO("Running query...", query, cgiEnv->GetInformation().ToJson());

        result r = txn.exec(query);

        if (!r.empty()) {
            if (checkbox->isChecked()) {
                Pool::Database().Update("CONTACTS",
                                         "1",
                                         "1",
                                         "is_default=?",
                                         { "FALSE" });
            }

            Pool::Database().Update("CONTACTS",
                                     "recipient",
                                     recipient,
                                     "is_default=?",
                                     { boost::lexical_cast<string>(checkbox->isChecked()) });
        }

        FillContactsDataTable();
    }

    catch (const pqxx::sql_error &ex) {
        LOG_ERROR(ex.what(), ex.query(), cgiEnv->GetInformation().ToJson());
    }

    catch (const boost::exception &ex) {
        LOG_ERROR(boost::diagnostic_information(ex), cgiEnv->GetInformation().ToJson());
    }

    catch (const std::exception &ex) {
        LOG_ERROR(ex.what(), cgiEnv->GetInformation().ToJson());
    }

    catch (...) {
        LOG_ERROR(UNKNOWN_ERROR, cgiEnv->GetInformation().ToJson());
    }
}

void CmsContacts::Impl::OnEraseButtonPressed(Wt::WPushButton *button)
{
    CgiRoot *cgiRoot = static_cast<CgiRoot *>(WApplication::instance());
    CgiEnv *cgiEnv = cgiRoot->GetCgiEnvInstance();

    try {
        WString dbKey(button->attributeValue("db-key"));

        WString question;
        if (cgiEnv->GetInformation().Client.Language.Code
                == CgiEnv::InformationRecord::ClientRecord::LanguageCode::Fa) {
            auto conn = Pool::Database().Connection();
            conn->activate();
            pqxx::work txn(*conn.get());

            string query((format("SELECT recipient_fa FROM \"%1%\""
                                 " WHERE recipient = %2%;")
                          % Pool::Database().GetTableName("CONTACTS")
                          % txn.quote(dbKey.toUTF8())).str());
            LOG_INFO("Running query...", query, cgiEnv->GetInformation().ToJson());

            result r = txn.exec(query);

            if (!r.empty()) {
                const pqxx::row row(r[0]);
                const string recipient_fa(row["recipient_fa"].c_str());
                question = tr("cms-contacts-erase-confirm-question").arg(WString::fromUTF8(recipient_fa));
            }
        } else {
            question = tr("cms-contacts-erase-confirm-question").arg(dbKey);
        }

        EraseMessageBox =
                std::make_unique<WMessageBox>(tr("cms-contacts-erase-confirm-title"),
                                              question, Warning, NoButton);
        EraseMessageBox->setAttributeValue("db-key", dbKey);
        EraseMessageBox->addButton(tr("cms-contacts-erase-confirm-ok"), Ok);
        EraseMessageBox->addButton(tr("cms-contacts-erase-confirm-cancel"), Cancel);

        EraseMessageBox->buttonClicked().connect(this, &CmsContacts::Impl::OnEraseDialogClosed);

        EraseMessageBox->show();
    }

    catch (const pqxx::sql_error &ex) {
        LOG_ERROR(ex.what(), ex.query(), cgiEnv->GetInformation().ToJson());
    }

    catch (const boost::exception &ex) {
        LOG_ERROR(boost::diagnostic_information(ex), cgiEnv->GetInformation().ToJson());
    }

    catch (const std::exception &ex) {
        LOG_ERROR(ex.what(), cgiEnv->GetInformation().ToJson());
    }

    catch (...) {
        LOG_ERROR(UNKNOWN_ERROR, cgiEnv->GetInformation().ToJson());
    }
}

void CmsContacts::Impl::OnEraseDialogClosed(Wt::StandardButton button)
{
    CgiRoot *cgiRoot = static_cast<CgiRoot *>(WApplication::instance());
    CgiEnv *cgiEnv = cgiRoot->GetCgiEnvInstance();

    try {
        if (button == Ok) {
            string recipient(EraseMessageBox->attributeValue("db-key").toUTF8());

            auto conn = Pool::Database().Connection();
            conn->activate();
            pqxx::work txn(*conn.get());

            string query((format("SELECT recipient FROM \"%1%\""
                                 " WHERE recipient = %2%;")
                          % Pool::Database().GetTableName("CONTACTS")
                          % txn.quote(recipient)).str());
            LOG_INFO("Running query...", query, cgiEnv->GetInformation().ToJson());

            result r = txn.exec(query);

            if (!r.empty()) {
                Pool::Database().Delete("CONTACTS", "recipient", recipient);
            }
            FillContactsDataTable();
        }
    }

    catch (const pqxx::sql_error &ex) {
        LOG_ERROR(ex.what(), ex.query(), cgiEnv->GetInformation().ToJson());
    }

    catch (const boost::exception &ex) {
        LOG_ERROR(boost::diagnostic_information(ex), cgiEnv->GetInformation().ToJson());
    }

    catch (const std::exception &ex) {
        LOG_ERROR(ex.what(), cgiEnv->GetInformation().ToJson());
    }

    catch (...) {
        LOG_ERROR(UNKNOWN_ERROR, cgiEnv->GetInformation().ToJson());
    }

    EraseMessageBox.reset();
}

void CmsContacts::Impl::FillContactsDataTable()
{
    ContactsTableContainer->clear();

    WTable *table = new WTable(ContactsTableContainer);
    table->setStyleClass("table table-striped table-hover");
    table->setHeaderCount(1, Orientation::Horizontal);

    table->elementAt(0, 0)->addWidget(new WText(tr("cms-contacts-recipient-name-en")));
    table->elementAt(0, 1)->addWidget(new WText(tr("cms-contacts-recipient-name-fa")));
    table->elementAt(0, 2)->addWidget(new WText(tr("cms-contacts-email-address")));
    table->elementAt(0, 3)->addWidget(new WText(tr("cms-contacts-is-default-recipient")));
    table->elementAt(0, 4)->addWidget(new WText(tr("cms-contacts-erase")));

    CgiRoot *cgiRoot = static_cast<CgiRoot *>(WApplication::instance());
    CgiEnv *cgiEnv = cgiRoot->GetCgiEnvInstance();

    try {
        auto conn = Pool::Database().Connection();
        conn->activate();
        pqxx::work txn(*conn.get());

        string query((format("SELECT recipient, recipient_fa, address, is_default"
                             " FROM \"%1%\" ORDER BY recipient ASC;")
                      % Pool::Database().GetTableName("CONTACTS")).str());
        LOG_INFO("Running query...", query, cgiEnv->GetInformation().ToJson());

        result r = txn.exec(query);

        int i = 0;
        for (const auto & row : r) {
            ++i;

            string recipient(row["recipient"].c_str());
            string recipient_fa(row["recipient_fa"].c_str());
            string address(row["address"].c_str());
            string is_default(row["is_default"].c_str());

            WSignalMapper<WInPlaceEdit *> *cellSignalMapper = new WSignalMapper<WInPlaceEdit *>(this);
            cellSignalMapper->mapped().connect(this, &CmsContacts::Impl::OnCellSaveButtonPressed);

            table->elementAt(i, 0)->addWidget(GetContactsCell(recipient, recipient, "recipient", cellSignalMapper));
            table->elementAt(i, 1)->addWidget(GetContactsCell(recipient_fa, recipient, "recipient_fa", cellSignalMapper));
            table->elementAt(i, 2)->addWidget(GetContactsCell(address, recipient, "address", cellSignalMapper));

            WSignalMapper<WCheckBox *> *setDefaultSignalMapper = new WSignalMapper<WCheckBox *>(this);
            setDefaultSignalMapper->mapped().connect(this, &CmsContacts::Impl::OnSetDefaultCheckBoxStateChanged);
            WCheckBox *setDefaultCheckBox = new WCheckBox();
            setDefaultSignalMapper->mapConnect(setDefaultCheckBox->checked(), setDefaultCheckBox);
            setDefaultSignalMapper->mapConnect(setDefaultCheckBox->unChecked(), setDefaultCheckBox);
            setDefaultCheckBox->setStyleClass("checkbox");
            setDefaultCheckBox->setAttributeValue("db-key", WString::fromUTF8(recipient));
            setDefaultCheckBox->setChecked(Database::IsTrue(is_default));
            table->elementAt(i, 3)->addWidget(setDefaultCheckBox);

            WSignalMapper<WPushButton *> *eraseSignalMapper = new WSignalMapper<WPushButton *>(this);
            eraseSignalMapper->mapped().connect(this, &CmsContacts::Impl::OnEraseButtonPressed);
            WPushButton *eraseButton = new WPushButton(tr("cms-contacts-erase-mark"));
            eraseSignalMapper->mapConnect(eraseButton->clicked(), eraseButton);
            eraseButton->setStyleClass("btn btn-default");
            eraseButton->setAttributeValue("db-key", WString::fromUTF8(recipient));
            table->elementAt(i, 4)->addWidget(eraseButton);
        }

        if (i != 0) {
            IsDefaultRecipientCheckBox->setChecked(false);
        } else {
            IsDefaultRecipientCheckBox->setChecked(true);
        }
    }

    catch (const pqxx::sql_error &ex) {
        LOG_ERROR(ex.what(), ex.query(), cgiEnv->GetInformation().ToJson());
    }

    catch (const boost::exception &ex) {
        LOG_ERROR(boost::diagnostic_information(ex), cgiEnv->GetInformation().ToJson());
    }

    catch (const std::exception &ex) {
        LOG_ERROR(ex.what(), cgiEnv->GetInformation().ToJson());
    }

    catch (...) {
        LOG_ERROR(UNKNOWN_ERROR, cgiEnv->GetInformation().ToJson());
    }
}

Wt::WInPlaceEdit *CmsContacts::Impl::GetContactsCell(const std::string &cellValue,
                                                     const std::string &dbKey,
                                                     const std::string &dbField,
                                                     Wt::WSignalMapper<Wt::WInPlaceEdit *> *signalMapper)
{
    WInPlaceEdit *edit = new WInPlaceEdit(WString::fromUTF8(cellValue));
    edit->setStyleClass("inplace");
    edit->saveButton()->setText(tr("cms-contacts-edit-save"));
    edit->cancelButton()->setText(tr("cms-contacts-edit-cancel"));
    edit->saveButton()->setStyleClass("btn btn-default");
    edit->cancelButton()->setStyleClass("btn btn-default");
    edit->setAttributeValue("db-key", WString::fromUTF8(dbKey));
    edit->setAttributeValue("db-field", WString::fromUTF8(dbField));
    signalMapper->mapConnect(edit->valueChanged(), edit);

    if (dbField == "recipient" || dbField == "recipient_fa") {
        WLengthValidator *validator = new WLengthValidator(Pool::Storage().MinEmailRecipientNameLength(),
                                                           Pool::Storage().MaxEmailRecipientNameLength());
        validator->setMandatory(true);
        edit->lineEdit()->setValidator(validator);
    } else {
        WRegExpValidator *validator = new WRegExpValidator(Pool::Storage().RegexEmail());
        validator->setFlags(MatchCaseInsensitive);
        validator->setMandatory(true);
        edit->lineEdit()->setValidator(validator);
    }

    return edit;
}
