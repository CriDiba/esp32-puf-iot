import {
  IoTDataPlaneClient,
  PublishCommand,
} from "@aws-sdk/client-iot-data-plane";
import {
  IoTClient,
  ListPrincipalThingsCommand,
  ListAuditFindingsCommand,
  RegisterCertificateCommand,
  AttachPolicyCommand,
} from "@aws-sdk/client-iot";
import { verifyCSR } from "./csr.mjs";
import { IOT_ENDPOINT, IOT_POLICY_NAME, IOT_CERT_PREFIX } from "./config.mjs";

const iotClient = new IoTClient({ region: "eu-west-1" });

const iotDataclient = new IoTDataPlaneClient({
  endpoint: "https://" + IOT_ENDPOINT,
});

export const handler = async (event) => {
  console.log(event);

  // TODO: check for taskId
  if ("certificateId" in event) {
    handleCertificateExpiring(event.certificateId);
  }

  if ("clientid" in event && "csr" in event) {
    // csr response
    const { clientid, csr, register, manual } = event;

    // generate crt from csr
    const certificatePem = await verifyCSR(csr);

    if (register) {
      // register new certificate
      const registerCertCmd = new RegisterCertificateCommand({
        certificatePem,
        setAsActive: true,
      });
      const { certificateId } = await iotClient.send(registerCertCmd);

      // attach policy
      const attachPolicyCmd = new AttachPolicyCommand({
        policyName: IOT_POLICY_NAME,
        target: `${IOT_CERT_PREFIX}/${certificateId}`,
      });
      await iotClient.send(attachPolicyCmd);
    }

    if (manual) {
      // return from manual invocation
      return certificatePem;
    } else {
      // send new certificate to device
      await mqttPublish(
        `management/${clientid}/crt`,
        JSON.stringify({ certificatePem })
      );
    }
  }
};

async function handleCertificateExpiring(certificateId) {
  // const listAuditCmd = new ListAuditFindingsCommand({
  //     taskId
  // })
  // const listAuditRes = await iotClient.send(listAuditCmd);
  // const certificateId = ...;

  // list things with specific certificate
  const listThingsCmd = new ListPrincipalThingsCommand({
    principal: `${IOT_CERT_PREFIX}/${certificateId}`,
  });
  const listThingsRes = await iotClient.send(listThingsCmd);

  // get client id
  const clientId = listThingsRes.things[0];

  // publish new csr request for client
  await mqttPublish(`management/${clientId}/csr_req`, "{}");
}

async function mqttPublish(topic, payload) {
  const settings = {
    topic,
    payload,
    qos: 0,
  };

  const command = new PublishCommand(settings);
  const response = await iotDataclient.send(command);
  return response;
}
