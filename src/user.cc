#include "user.h"

#include "proto/message.pb.h"

#include "conn.h"

User::User(Conn& conn, UserRepo& user_repo)
    : conn_(conn), user_repo_(user_repo) {}

void User::OnMessage(MessagePtr bin) {
  req_.Clear();
  if (!req_.ParseFromArray(bin->data(), bin->size())) {
    SendError(ErrType::Error_Type_InvalidRequest);
    return;
  }

  if (!authenticated_) {
    if (req_.type() == req_.Authentication) {
      assert(req_.has_authentication() == true);
      Authenticate(req_.authentication());
      return;
    } else {
      SendError(ErrType::Error_Type_MustAuthenticateFirst);
      return;
    }
  } else if (req_.type() == req_.Authentication) {
    SendError(ErrType::Error_Type_AlreadyAuthenticated);
    return;
  }

  assert(authenticated_ == true);
  assert(req_.type() != req_.Authentication);

  if (req_.type() == req_.Message) {
    assert(req_.has_message() == true);
    if (!user_repo_.Contains(req_.message().recipient())) {
      SendError(ErrType::Error_Type_UserNotOnline);
      return;
    }

    proto::Message msg;
    msg.Swap(req_.mutable_message());
    auto recipient = user_repo_.Get(msg.recipient());
    msg.set_sender(name_);
    recipient.SendMessage(msg);
  } else if (req_.type() == req_.MessagesListReq) {
    // return messages
  }
}

void User::Authenticate(const proto::Authentication& auth) {
  if (!user_repo_.Register(auth.name(), this)) {
    SendError(ErrType::Error_Type_UsernameTaken);
    return;
  }
  name_ = auth.name();
  authenticated_ = true;
}

void User::SendError(ErrType type) {
  res_.set_type(res_.Error);
  res_.mutable_error()->set_type(type);
  SendResponse();
}

void User::SendMessage(const proto::Message& msg) {
  res_.set_type(res_.Message);
  res_.set_allocated_message(const_cast<proto::Message*>(&msg));
  SendResponse();
  res_.release_message();
}

void User::SendResponse() {
  auto bin = std::make_unique<Conn::Message>(res_.ByteSize());
  res_.SerializeToArray(bin->data(), bin->size());
  conn_.Send(std::move(bin));
  res_.Clear();
}

void User::OnDisconnect() { user_repo_.Remove(name_); }
