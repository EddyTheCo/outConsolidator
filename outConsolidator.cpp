// include https://eddytheco.github.io/Qed25519/
#include"crypto/qed25519.hpp"
// include https://eddytheco.github.io/Qslip10/
#include"crypto/qslip10.hpp"
// include https://eddytheco.github.io/QAddrBundle/
#include"qaddr_bundle.hpp"

// include https://eddytheco.github.io/QclientMqtt-IOTA/
#include"client/qclientMQTT.hpp"

#include<QTimer>
#include <QCoreApplication>
#include <queue>

using namespace qiota::qblocks;
using namespace qiota;
using namespace qcrypto;
using namespace qencoding::qbech32::Iota;


class Book_Server : public QObject
{
    Q_OBJECT
signals:
    void stateChanged(void);
    void newBook(QJsonValue);
public:
    void changeState(bool st)
    {
        if(st!=publishing)
        {
            publishing=!publishing;
            emit stateChanged();
        }
    }
    std::queue<QJsonValue> queue;
    bool publishing;
    Book_Server(QObject *parent = nullptr);
};

Book_Server::Book_Server(QObject *parent):QObject(parent),publishing(false)
{
    QObject::connect(this,&Book_Server::stateChanged,this,[=](){
        if(queue.size()&&!publishing)
        {
            const auto var=queue.front();
            queue.pop();
            emit newBook(var);
        }
    });
}

int main(int argc, char** argv)
{
    // Create the needed https://doc.qt.io/qt-6/qcoreapplication.html
    auto a=QCoreApplication(argc, argv);

    if(argc>3)
    {
        auto seed=QByteArray::fromHex(argv[1]);
        quint32 indexDust=atoi(argv[3]);
        quint32 indexCons=atoi(argv[2]);

        auto MK=Master_key(seed);
        QVector<quint32> pathCons={44,4219,0,0,indexCons};
        QVector<quint32> pathDust={44,4219,0,0,indexDust};
        auto keysCons=MK.slip10_key_from_path(pathCons);
        auto keysDust=MK.slip10_key_from_path(pathDust);


        auto iota_client=new Client(&a);

        iota_client->set_node_address(QUrl(argv[4]));

        const QString readdress=QString(argv[5]);

        // The third argument will be taken as the node JWT for protected routes
        if(argc>5)iota_client->set_jwt(QString(argv[6]));

        auto info=iota_client->get_api_core_v2_info();
        QObject::connect(info,&Node_info::finished,&a,[=,&a]( ){


            auto mqtt_client=new ClientMqtt(&a);

            QObject::connect(mqtt_client,&QMqttClient::stateChanged,&a,[=,&a]
            {
                static bool entered=true;
                if(mqtt_client->state()==QMqttClient::Disconnected)
                {
                    qDebug()<<"MQTT connection dropped";
                    a.quit();
                }
                if(mqtt_client->state()==QMqttClient::Connected&&entered)
                {
                    entered=false;
                    const auto cons_address=AddressBundle(qed25519::create_keypair(keysCons.secret_key())).get_address_bech32(info->bech32Hrp);
                    const auto dust_address=AddressBundle(qed25519::create_keypair(keysDust.secret_key())).get_address_bech32(info->bech32Hrp);

                    qDebug()<<"Consolidation address:\n"<<cons_address;
                    qDebug()<<"Dust address:\n"<<dust_address;

                    auto booker=new Book_Server(&a);
                    auto resp=mqtt_client->get_outputs_unlock_condition_address("address/"+dust_address);

                    auto lambda= [=,&a](QJsonValue data)
                    {
                        if(!booker->publishing)
                        {
                            booker->changeState(true);
                            QTimer::singleShot(15000,&a,[=](){booker->changeState(false);});
                            auto node_outputs_=new Node_outputs();

                            QObject::connect(node_outputs_,&Node_outputs::finished,&a,[=,&a]( ){
                                auto outs=std::vector<Node_output>{Node_output(data)};
                                auto cons_bundle=AddressBundle(qed25519::create_keypair(keysCons.secret_key()));
                                auto dust_bundle=AddressBundle(qed25519::create_keypair(keysDust.secret_key()));
                                dust_bundle.consume_outputs(outs);
                                if(dust_bundle.amount)
                                {
                                    cons_bundle.consume_outputs(node_outputs_->outs_);
                                    const auto conUnlCon=Unlock_Condition::Address(cons_bundle.get_address());
                                    const auto minDeposit=Client::get_deposit(Output::Basic(0,{conUnlCon}),info);
                                    if(dust_bundle.amount+cons_bundle.amount>=minDeposit)
                                    {
                                        pvector<const Output> the_outputs_;
                                        auto ConsOut= Output::Basic(dust_bundle.amount+cons_bundle.amount,
                                                                    {conUnlCon},{},{});
                                        auto RecPair= decode(readdress);
                                        if(RecPair.second.size()&&RecPair.first==info->bech32Hrp&&
                                                dust_bundle.amount+cons_bundle.amount>=2*minDeposit)
                                        {
                                            ConsOut->amount_=minDeposit;
                                            const auto RecUnlCon=Unlock_Condition::Address(Address::from_array(RecPair.second));
                                            auto RecOut= Output::Basic(dust_bundle.amount+cons_bundle.amount-minDeposit,
                                                                       {RecUnlCon},{},{});
                                            the_outputs_.push_back(RecOut);
                                        }
                                        the_outputs_.push_back(ConsOut);
                                        the_outputs_.insert(the_outputs_.end(), dust_bundle.ret_outputs.begin(), dust_bundle.ret_outputs.end());
                                        the_outputs_.insert(the_outputs_.end(), cons_bundle.ret_outputs.begin(), cons_bundle.ret_outputs.end());


                                        auto Inputs_Commitment=Block::get_inputs_Commitment(dust_bundle.Inputs_hash+cons_bundle.Inputs_hash);
                                        auto inputs=dust_bundle.inputs;
                                        inputs.insert(inputs.end(),cons_bundle.inputs.begin(),cons_bundle.inputs.end());
                                        auto essence=Essence::Transaction(info->network_id_,inputs,Inputs_Commitment,the_outputs_);

                                        dust_bundle.create_unlocks(essence->get_hash());
                                        auto unlocks=dust_bundle.unlocks;
                                        cons_bundle.create_unlocks(essence->get_hash(),unlocks.size());
                                        unlocks.insert(unlocks.end(),cons_bundle.unlocks.begin(),cons_bundle.unlocks.end());

                                        auto trpay=Payload::Transaction(essence,unlocks);
                                        auto block_=Block(trpay);
                                        auto resp=mqtt_client->get_subscription("transactions/"+trpay->get_id().toHexString() +"/included-block");
                                        QObject::connect(resp,&ResponseMqtt::returned,&a,[=](auto var){
                                            booker->changeState(false);
                                            resp->deleteLater();
                                        });

                                        iota_client->send_block(block_);
                                    }
                                    node_outputs_->deleteLater();
                                }
                            });
                            iota_client->get_outputs<Output::Basic_typ>(node_outputs_,"address="+cons_address);
                        }
                        else
                        {
                            booker->queue.push(data);
                        }

                    };
                    QObject::connect(booker,&Book_Server::newBook,&a,lambda);
                    QObject::connect(resp,&ResponseMqtt::returned,&a,lambda);
                }
            });
            mqtt_client->set_node_address(QUrl(argv[4]));
        });

        return a.exec();

    }
}
#include "outConsolidator.moc"
