import {
  AttachPolicyCommand,
  IoTClient,
  ListPrincipalThingsCommand,
  ListThingPrincipalsCommand,
  RegisterCertificateCommand,
  UpdateCertificateCommand,
} from "@aws-sdk/client-iot";
import {
  IoTDataPlaneClient,
  PublishCommand,
} from "@aws-sdk/client-iot-data-plane";
import { IOT_CERT_PREFIX, IOT_ENDPOINT, IOT_POLICY_NAME } from "./config.mjs";
import { verifyCSR } from "./csr.mjs";

const iotClient = new IoTClient({ region: "eu-west-1" });

const iotDataclient = new IoTDataPlaneClient({
  endpoint: "https://" + IOT_ENDPOINT,
});

export const handler = async (event) => {
  console.log(event);

  if ("certificateId" in event && "refresh" in event) {
    await handleCertificateExpiring(event.certificateId);
  }

  if ("clientid" in event && "csr" in event) {
    await handleSignCsr(event);
  }

  if ("clientid" in event && "certificateId" in event && "ack" in event) {
    await handleCertificateAck(event);
  }
};

async function handleSignCsr(event) {
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
      JSON.stringify({ certificatePem, length: certificatePem.length })
    );
  }
}

async function handleCertificateExpiring(certificateId) {
  // TODO: handle certificate expiring from taskId of AWS Iot Device Defender
  // const listAuditCmd = new ListAuditFindingsCommand({
  //     taskId
  // })
  // const listAuditRes = await iotClient.send(listAuditCmd);
  // const certificateId = ...;

  const principal = `${IOT_CERT_PREFIX}/${certificateId}`;
  console.log(`list thigs attached to certificate ${principal}`);

  // list things with specific certificate
  const listThingsCmd = new ListPrincipalThingsCommand({
    principal: principal,
  });
  const listThingsRes = await iotClient.send(listThingsCmd);

  // get client id
  const clientId = listThingsRes.things[0];
  console.log(`start certificate refresh request for client ${clientId}`);

  // publish new csr request for client
  await mqttPublish(`management/${clientId}/csr_req`, "{}");
}

async function handleCertificateAck(event) {
  const { clientid, certificateId } = event;

  // list all principals for a things
  const listThingPrincipalsCmd = new ListThingPrincipalsCommand({
    thingName: clientid,
  });
  const listThingPrincipalsRes = await iotClient.send(listThingPrincipalsCmd);

  for (const principal of listThingPrincipalsRes.principals) {
    if (
      principal.startsWith(IOT_CERT_PREFIX) &&
      !principal.endsWith(certificateId)
    ) {
      const oldCertificateId = principal.substring(principal.indexOf("/") + 1);

      // revoke old certificates
      const updateCertificateCmd = new UpdateCertificateCommand({
        certificateId: oldCertificateId,
        newStatus: "REVOKED",
      });
      await iotClient.send(updateCertificateCmd);
    }
  }
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
