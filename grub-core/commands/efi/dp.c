#include <grub/dl.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/efi/disk.h>
#include <grub/device.h>
#include <grub/charset.h>
#include <grub/err.h>
#include <grub/extcmd.h>
#include <grub/file.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/net.h>
#include <grub/types.h>
#include <grub/term.h>

GRUB_MOD_LICENSE ("GPLv3+");

static char *
dump_vendor_path (grub_efi_vendor_device_path_t *vendor)
{
  grub_uint32_t vendor_data_len = vendor->header.length - sizeof (*vendor);
  char data[4];
  char *str = NULL;
  str = grub_malloc (55 + vendor_data_len * 3);
  grub_snprintf (str, 55,
           "Vendor(%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x)[%x: ",
           (unsigned) vendor->vendor_guid.data1,
           (unsigned) vendor->vendor_guid.data2,
           (unsigned) vendor->vendor_guid.data3,
           (unsigned) vendor->vendor_guid.data4[0],
           (unsigned) vendor->vendor_guid.data4[1],
           (unsigned) vendor->vendor_guid.data4[2],
           (unsigned) vendor->vendor_guid.data4[3],
           (unsigned) vendor->vendor_guid.data4[4],
           (unsigned) vendor->vendor_guid.data4[5],
           (unsigned) vendor->vendor_guid.data4[6],
           (unsigned) vendor->vendor_guid.data4[7],
           vendor_data_len);
  if (vendor->header.length > sizeof (*vendor))
  {
    grub_uint32_t i;
    for (i = 0; i < vendor_data_len; i++)
    {
      grub_snprintf (data, 4, "%02x ", vendor->vendor_defined_data[i]);
      grub_strcpy (str + grub_strlen (str), data);
    }
  }
  grub_strcpy (str + grub_strlen (str), "]");
  return str;
}

char *
grub_efi_device_path_to_str (grub_efi_device_path_t *dp)
{
  char *text_dp = NULL;
  while (1)
  {
    char *node = NULL;
    grub_efi_uint8_t type = GRUB_EFI_DEVICE_PATH_TYPE (dp);
    grub_efi_uint8_t subtype = GRUB_EFI_DEVICE_PATH_SUBTYPE (dp);
    grub_efi_uint16_t len = GRUB_EFI_DEVICE_PATH_LENGTH (dp);
    switch (type)
    {
      case GRUB_EFI_END_DEVICE_PATH_TYPE:
        switch (subtype)
        {
          case GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE:
            node = grub_xasprintf ("/EndEntire");
            break;
          case GRUB_EFI_END_THIS_DEVICE_PATH_SUBTYPE:
            node = grub_xasprintf ("/EndThis");
            break;
          default:
            node = grub_xasprintf ("/EndUnknown(%x)", (unsigned) subtype);
            break;
        }
        break;

      case GRUB_EFI_HARDWARE_DEVICE_PATH_TYPE:
        switch (subtype)
        {
          case GRUB_EFI_PCI_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_pci_device_path_t *pci =
                    (grub_efi_pci_device_path_t *) dp;
            node = grub_xasprintf ("/PCI(%x,%x)",
                        (unsigned) pci->function, (unsigned) pci->device);
          }
            break;
          case GRUB_EFI_PCCARD_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_pccard_device_path_t *pccard =
                    (grub_efi_pccard_device_path_t *) dp;
            node = grub_xasprintf ("/PCCARD(%x)", (unsigned) pccard->function);
          }
            break;
          case GRUB_EFI_MEMORY_MAPPED_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_memory_mapped_device_path_t *mmapped =
                    (grub_efi_memory_mapped_device_path_t *) dp;
            node = grub_xasprintf ("/MMap(%x,%llx,%llx)",
                          (unsigned) mmapped->memory_type,
                          (unsigned long long) mmapped->start_address,
                          (unsigned long long) mmapped->end_address);
          }
            break;
          case GRUB_EFI_VENDOR_DEVICE_PATH_SUBTYPE:
          {
            char *tmp_str = NULL;
            tmp_str = dump_vendor_path ((grub_efi_vendor_device_path_t *) dp);
            node = grub_xasprintf ("/Hardware%s", tmp_str);
            if (tmp_str)
              grub_free (tmp_str);
          }
            break;
          case GRUB_EFI_CONTROLLER_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_controller_device_path_t *controller =
                    (grub_efi_controller_device_path_t *) dp;
            node = grub_xasprintf ("/Ctrl(%x)",
                                   (unsigned) controller->controller_number);
          }
            break;
          default:
            node = grub_xasprintf ("/UnknownHW(%x)", (unsigned) subtype);
            break;
        }
        break;

      case GRUB_EFI_ACPI_DEVICE_PATH_TYPE:
        switch (subtype)
        {
          case GRUB_EFI_ACPI_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_acpi_device_path_t *acpi =
                    (grub_efi_acpi_device_path_t *) dp;
            node = grub_xasprintf ("/ACPI(%x,%x)",
                                   (unsigned) acpi->hid, (unsigned) acpi->uid);
          }
            break;
          case GRUB_EFI_EXPANDED_ACPI_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_expanded_acpi_device_path_t *eacpi =
                    (grub_efi_expanded_acpi_device_path_t *) dp;
            char *hidstr = NULL, *uidstr = NULL, *cidstr = NULL;

            if (GRUB_EFI_EXPANDED_ACPI_HIDSTR (dp)[0] == '\0')
              hidstr = grub_xasprintf ("%x,", (unsigned) eacpi->hid);
            else
              hidstr = grub_xasprintf ("%s,", GRUB_EFI_EXPANDED_ACPI_HIDSTR (dp));

            if (GRUB_EFI_EXPANDED_ACPI_UIDSTR (dp)[0] == '\0')
              uidstr = grub_xasprintf ("%x,", (unsigned) eacpi->uid);
            else
              uidstr = grub_xasprintf ("%s,", GRUB_EFI_EXPANDED_ACPI_UIDSTR (dp));

            if (GRUB_EFI_EXPANDED_ACPI_CIDSTR (dp)[0] == '\0')
              cidstr = grub_xasprintf ("%x)", (unsigned) eacpi->cid);
            else
              cidstr = grub_xasprintf ("%s)", GRUB_EFI_EXPANDED_ACPI_CIDSTR (dp));
            node = grub_xasprintf ("/ACPI(%s%s%s", hidstr, uidstr, cidstr);
            if (hidstr) grub_free (hidstr);
            if (uidstr) grub_free (uidstr);
            if (cidstr) grub_free (cidstr);
          }
            break;
          default:
            node = grub_xasprintf ("/UnknownACPI(%x)", (unsigned) subtype);
            break;
        }
        break;

      case GRUB_EFI_MESSAGING_DEVICE_PATH_TYPE:
        switch (subtype)
        {
          case GRUB_EFI_ATAPI_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_atapi_device_path_t *atapi =
                    (grub_efi_atapi_device_path_t *) dp;
            node = grub_xasprintf ("/ATAPI(%x,%x,%x)",
                    (unsigned) atapi->primary_secondary,
                    (unsigned) atapi->slave_master,
                    (unsigned) atapi->lun);
          }
            break;
          case GRUB_EFI_SCSI_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_scsi_device_path_t *scsi =
                    (grub_efi_scsi_device_path_t *) dp;
            node = grub_xasprintf ("/SCSI(%x,%x)",
                  (unsigned) scsi->pun,
                  (unsigned) scsi->lun);
          }
            break;
          case GRUB_EFI_FIBRE_CHANNEL_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_fibre_channel_device_path_t *fc =
                    (grub_efi_fibre_channel_device_path_t *) dp;
            node = grub_xasprintf ("/FibreChannel(%llx,%llx)",
                  (unsigned long long) fc->wwn,
                  (unsigned long long) fc->lun);
          }
            break;
          case GRUB_EFI_1394_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_1394_device_path_t *firewire =
                    (grub_efi_1394_device_path_t *) dp;
            node = grub_xasprintf ("/1394(%llx)", (unsigned long long)firewire->guid);
          }
            break;
          case GRUB_EFI_USB_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_usb_device_path_t *usb =
                    (grub_efi_usb_device_path_t *) dp;
            node = grub_xasprintf ("/USB(%x,%x)",
                  (unsigned) usb->parent_port_number,
                  (unsigned) usb->usb_interface);
          }
            break;
          case GRUB_EFI_USB_CLASS_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_usb_class_device_path_t *usb_class =
                    (grub_efi_usb_class_device_path_t *) dp;
            node = grub_xasprintf ("/USBClass(%x,%x,%x,%x,%x)",
                  (unsigned) usb_class->vendor_id,
                  (unsigned) usb_class->product_id,
                  (unsigned) usb_class->device_class,
                  (unsigned) usb_class->device_subclass,
                  (unsigned) usb_class->device_protocol);
          }
            break;
          case GRUB_EFI_I2O_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_i2o_device_path_t *i2o =
                    (grub_efi_i2o_device_path_t *) dp;
            node = grub_xasprintf ("/I2O(%x)", (unsigned) i2o->tid);
          }
            break;
          case GRUB_EFI_MAC_ADDRESS_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_mac_address_device_path_t *mac =
                    (grub_efi_mac_address_device_path_t *) dp;
            node = grub_xasprintf ("/MacAddr(%02x:%02x:%02x:%02x:%02x:%02x,%x)",
                  (unsigned) mac->mac_address[0],
                  (unsigned) mac->mac_address[1],
                  (unsigned) mac->mac_address[2],
                  (unsigned) mac->mac_address[3],
                  (unsigned) mac->mac_address[4],
                  (unsigned) mac->mac_address[5],
                  (unsigned) mac->if_type);
          }
            break;
          case GRUB_EFI_IPV4_DEVICE_PATH_SUBTYPE:
          {
            char ipv4_dp1[60];
            char ipv4_dp2[35];
            grub_efi_ipv4_device_path_t *ipv4 =
                    (grub_efi_ipv4_device_path_t *) dp;
            grub_snprintf (ipv4_dp1, 60,
                  "(%u.%u.%u.%u,%u.%u.%u.%u,%u,%u,%4x,%2x",
                  (unsigned) ipv4->local_ip_address[0],
                  (unsigned) ipv4->local_ip_address[1],
                  (unsigned) ipv4->local_ip_address[2],
                  (unsigned) ipv4->local_ip_address[3],
                  (unsigned) ipv4->remote_ip_address[0],
                  (unsigned) ipv4->remote_ip_address[1],
                  (unsigned) ipv4->remote_ip_address[2],
                  (unsigned) ipv4->remote_ip_address[3],
                  (unsigned) ipv4->local_port,
                  (unsigned) ipv4->remote_port,
                  (unsigned) ipv4->protocol,
                  (unsigned) ipv4->static_ip_address);
            if (len == sizeof (*ipv4))
            {
              grub_snprintf (ipv4_dp2, 35, ",%u.%u.%u.%u,%u.%u.%u.%u)",
                  (unsigned) ipv4->gateway_ip_address[0],
                  (unsigned) ipv4->gateway_ip_address[1],
                  (unsigned) ipv4->gateway_ip_address[2],
                  (unsigned) ipv4->gateway_ip_address[3],
                  (unsigned) ipv4->subnet_mask[0],
                  (unsigned) ipv4->subnet_mask[1],
                  (unsigned) ipv4->subnet_mask[2],
                  (unsigned) ipv4->subnet_mask[3]);
            }
            else
              grub_strcpy (ipv4_dp2, ")");
            node = grub_xasprintf ("/IPv4%s%s", ipv4_dp1, ipv4_dp2);
          }
            break;
          case GRUB_EFI_IPV6_DEVICE_PATH_SUBTYPE:
          {
            char ipv6_dp1[70];
            char ipv6_dp2[30];
            grub_efi_ipv6_device_path_t *ipv6 =
                    (grub_efi_ipv6_device_path_t *) dp;
            grub_snprintf (ipv6_dp1, 70,
                    "(%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x,%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x,%u,%u,%x,%x",
                  (unsigned) grub_be_to_cpu16 (ipv6->local_ip_address[0]),
                  (unsigned) grub_be_to_cpu16 (ipv6->local_ip_address[1]),
                  (unsigned) grub_be_to_cpu16 (ipv6->local_ip_address[2]),
                  (unsigned) grub_be_to_cpu16 (ipv6->local_ip_address[3]),
                  (unsigned) grub_be_to_cpu16 (ipv6->local_ip_address[4]),
                  (unsigned) grub_be_to_cpu16 (ipv6->local_ip_address[5]),
                  (unsigned) grub_be_to_cpu16 (ipv6->local_ip_address[6]),
                  (unsigned) grub_be_to_cpu16 (ipv6->local_ip_address[7]),
                  (unsigned) grub_be_to_cpu16 (ipv6->remote_ip_address[0]),
                  (unsigned) grub_be_to_cpu16 (ipv6->remote_ip_address[1]),
                  (unsigned) grub_be_to_cpu16 (ipv6->remote_ip_address[2]),
                  (unsigned) grub_be_to_cpu16 (ipv6->remote_ip_address[3]),
                  (unsigned) grub_be_to_cpu16 (ipv6->remote_ip_address[4]),
                  (unsigned) grub_be_to_cpu16 (ipv6->remote_ip_address[5]),
                  (unsigned) grub_be_to_cpu16 (ipv6->remote_ip_address[6]),
                  (unsigned) grub_be_to_cpu16 (ipv6->remote_ip_address[7]),
                  (unsigned) ipv6->local_port,
                  (unsigned) ipv6->remote_port,
                  (unsigned) ipv6->protocol,
                  (unsigned) ipv6->static_ip_address);
            if (len == sizeof (*ipv6))
            {
              grub_snprintf (ipv6_dp2, 30,
                            ",%u,%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x)",
              (unsigned) ipv6->prefix_length,
              (unsigned) grub_be_to_cpu16 (ipv6->gateway_ip_address[0]),
              (unsigned) grub_be_to_cpu16 (ipv6->gateway_ip_address[1]),
              (unsigned) grub_be_to_cpu16 (ipv6->gateway_ip_address[2]),
              (unsigned) grub_be_to_cpu16 (ipv6->gateway_ip_address[3]),
              (unsigned) grub_be_to_cpu16 (ipv6->gateway_ip_address[4]),
              (unsigned) grub_be_to_cpu16 (ipv6->gateway_ip_address[5]),
              (unsigned) grub_be_to_cpu16 (ipv6->gateway_ip_address[6]),
              (unsigned) grub_be_to_cpu16 (ipv6->gateway_ip_address[7]));
            }
            else
              grub_strcpy (ipv6_dp2, ")");
            node = grub_xasprintf ("/IPv6%s%s", ipv6_dp1, ipv6_dp2);
          }
            break;
          case GRUB_EFI_INFINIBAND_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_infiniband_device_path_t *ib =
                    (grub_efi_infiniband_device_path_t *) dp;
            node = grub_xasprintf ("/InfiniBand(%x,%llx,%llx,%llx)",
                  (unsigned) ib->port_gid[0], /* XXX */
                  (unsigned long long) ib->remote_id,
                  (unsigned long long) ib->target_port_id,
                  (unsigned long long) ib->device_id);
          }
            break;
          case GRUB_EFI_UART_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_uart_device_path_t *uart =
                    (grub_efi_uart_device_path_t *) dp;
            node = grub_xasprintf ("/UART(%llu,%u,%x,%x)",
                  (unsigned long long) uart->baud_rate,
                  uart->data_bits,
                  uart->parity,
                  uart->stop_bits);
          }
            break;
          case GRUB_EFI_SATA_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_sata_device_path_t *sata;
            sata = (grub_efi_sata_device_path_t *) dp;
            node = grub_xasprintf ("/Sata(%x,%x,%x)",
                  sata->hba_port,
                  sata->multiplier_port,
                  sata->lun);
          }
            break;

          case GRUB_EFI_VENDOR_MESSAGING_DEVICE_PATH_SUBTYPE:
          {
            char *tmp_str = NULL;
            tmp_str = dump_vendor_path ((grub_efi_vendor_device_path_t *) dp);
            node = grub_xasprintf ("/Messaging%s", tmp_str);
            if (tmp_str)
              grub_free (tmp_str);
          }
            break;
          case GRUB_EFI_URI_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_uri_device_path_t *uri = (grub_efi_uri_device_path_t *) dp;
            node = grub_xasprintf ("/URI(%s)", uri->uri);
          }
            break;
          case GRUB_EFI_DNS_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_dns_device_path_t *dns =
                    (grub_efi_dns_device_path_t *) dp;
            char ip_str[25];
            if (dns->is_ipv6)
            {
              grub_snprintf (ip_str, 25, "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
                (grub_uint16_t)(grub_be_to_cpu32(dns->dns_server_ip[0].addr[0]) >> 16),
                (grub_uint16_t)(grub_be_to_cpu32(dns->dns_server_ip[0].addr[0])),
                (grub_uint16_t)(grub_be_to_cpu32(dns->dns_server_ip[0].addr[1]) >> 16),
                (grub_uint16_t)(grub_be_to_cpu32(dns->dns_server_ip[0].addr[1])),
                (grub_uint16_t)(grub_be_to_cpu32(dns->dns_server_ip[0].addr[2]) >> 16),
                (grub_uint16_t)(grub_be_to_cpu32(dns->dns_server_ip[0].addr[2])),
                (grub_uint16_t)(grub_be_to_cpu32(dns->dns_server_ip[0].addr[3]) >> 16),
                (grub_uint16_t)(grub_be_to_cpu32(dns->dns_server_ip[0].addr[3])));
            }
            else
            {
              grub_snprintf (ip_str, 25, "%d.%d.%d.%d",
                dns->dns_server_ip[0].v4.addr[0],
                dns->dns_server_ip[0].v4.addr[1],
                dns->dns_server_ip[0].v4.addr[2],
                dns->dns_server_ip[0].v4.addr[3]);
            }
            node = grub_xasprintf ("/DNS(%s)", ip_str);
          }
            break;
          default:
            node = grub_xasprintf ("/UnknownMessaging(%x)", (unsigned) subtype);
            break;
        }
        break;

      case GRUB_EFI_MEDIA_DEVICE_PATH_TYPE:
        switch (subtype)
        {
          case GRUB_EFI_HARD_DRIVE_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_hard_drive_device_path_t *hd =
                    (grub_efi_hard_drive_device_path_t *) dp;
            node = grub_xasprintf
                  ("/HD(%u,%llx,%llx,%02x%02x%02x%02x%02x%02x%02x%02x,%x,%x)",
                  hd->partition_number,
                  (unsigned long long) hd->partition_start,
                  (unsigned long long) hd->partition_size,
                  (unsigned) hd->partition_signature[0],
                  (unsigned) hd->partition_signature[1],
                  (unsigned) hd->partition_signature[2],
                  (unsigned) hd->partition_signature[3],
                  (unsigned) hd->partition_signature[4],
                  (unsigned) hd->partition_signature[5],
                  (unsigned) hd->partition_signature[6],
                  (unsigned) hd->partition_signature[7],
                  (unsigned) hd->partmap_type,
                  (unsigned) hd->signature_type);
          }
            break;
          case GRUB_EFI_CDROM_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_cdrom_device_path_t *cd =
                    (grub_efi_cdrom_device_path_t *) dp;
            node = grub_xasprintf ("/CD(%u,%llx,%llx)",
                  cd->boot_entry,
                  (unsigned long long) cd->partition_start,
                  (unsigned long long) cd->partition_size);
          }
            break;
          case GRUB_EFI_VENDOR_MEDIA_DEVICE_PATH_SUBTYPE:
          {
            char *tmp_str = NULL;
            tmp_str = dump_vendor_path ((grub_efi_vendor_device_path_t *) dp);
            node = grub_xasprintf ("/Media%s", tmp_str);
            if (tmp_str)
              grub_free (tmp_str);
          }
            break;
          case GRUB_EFI_FILE_PATH_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_file_path_device_path_t *fp;
            grub_uint8_t *buf;
            fp = (grub_efi_file_path_device_path_t *) dp;
            buf = grub_zalloc ((len - 4) * 2 + 1);
            if (buf)
            {
              grub_efi_char16_t *dup_name = grub_zalloc (len - 4);
              if (!dup_name)
              {
                grub_errno = GRUB_ERR_NONE;
                node = grub_xasprintf ("/File((null))");
                grub_free (buf);
                break;
              }
              *grub_utf16_to_utf8 (buf, grub_memcpy (dup_name, fp->path_name, len - 4),
                        (len - 4) / sizeof (grub_efi_char16_t))
                = '\0';
              grub_free (dup_name);
            }
            else
              grub_errno = GRUB_ERR_NONE;
            node = grub_xasprintf ("/File(%s)", buf);
            grub_free (buf);
          }
            break;
          case GRUB_EFI_PROTOCOL_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_protocol_device_path_t *proto =
                    (grub_efi_protocol_device_path_t *) dp;
            node = grub_xasprintf
                  ("/Protocol(%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x)",
                  (unsigned) proto->guid.data1,
                  (unsigned) proto->guid.data2,
                  (unsigned) proto->guid.data3,
                  (unsigned) proto->guid.data4[0],
                  (unsigned) proto->guid.data4[1],
                  (unsigned) proto->guid.data4[2],
                  (unsigned) proto->guid.data4[3],
                  (unsigned) proto->guid.data4[4],
                  (unsigned) proto->guid.data4[5],
                  (unsigned) proto->guid.data4[6],
                  (unsigned) proto->guid.data4[7]);
          }
            break;
          default:
            node = grub_xasprintf ("/UnknownMedia(%x)", (unsigned) subtype);
            break;
          }
        break;

      case GRUB_EFI_BIOS_DEVICE_PATH_TYPE:
        switch (subtype)
        {
          case GRUB_EFI_BIOS_DEVICE_PATH_SUBTYPE:
          {
            grub_efi_bios_device_path_t *bios =
                    (grub_efi_bios_device_path_t *) dp;
            node = grub_xasprintf ("/BIOS(%x,%x,%s)",
                  (unsigned) bios->device_type,
                  (unsigned) bios->status_flags,
                  (char *) (dp + 1));
          }
            break;
          default:
            node = grub_xasprintf ("/UnknownBIOS(%x)", (unsigned) subtype);
            break;
          }
        break;

      default:
      {
        node = grub_xasprintf ("/UnknownType(%x,%x)", (unsigned) type,
                                (unsigned) subtype);
        char *str = text_dp;
        text_dp = grub_zalloc (grub_strlen (text_dp) + grub_strlen(node) + 1);
        if (str)
        {
          grub_strcpy (text_dp, str);
          grub_free (str);
        }
        grub_strcat (text_dp, node);
        if (node)
          grub_free (node);
        return text_dp;
      }
        break;
    }
    char *str = text_dp;
    text_dp = grub_zalloc (grub_strlen (text_dp) + grub_strlen(node) + 1);
    if (str)
    {
      grub_strcpy (text_dp, str);
      grub_free (str);
    }
    grub_strcat (text_dp, node);
    if (node)
      grub_free (node);
    if (GRUB_EFI_END_ENTIRE_DEVICE_PATH (dp))
      break;

    dp = (grub_efi_device_path_t *) ((char *) dp + len);
  }
  return text_dp;
}

static grub_efi_uintn_t
device_path_node_length (const void *node)
{
  return grub_get_unaligned16 ((grub_efi_uint16_t *)
                              &((grub_efi_device_path_protocol_t *)(node))->length);
}

static void
set_device_path_node_length (void *node, grub_efi_uintn_t len)
{
  grub_set_unaligned16 ((grub_efi_uint16_t *)
                        &((grub_efi_device_path_protocol_t *)(node))->length,
                        (grub_efi_uint16_t)(len));
}

grub_efi_uintn_t
grub_efi_get_dp_size (const grub_efi_device_path_protocol_t *dp)
{
  grub_efi_device_path_t *p;
  grub_efi_uintn_t total_size = 0;
  for (p = (grub_efi_device_path_t *) dp; ; p = GRUB_EFI_NEXT_DEVICE_PATH (p))
  {
    total_size += GRUB_EFI_DEVICE_PATH_LENGTH (p);
    if (GRUB_EFI_END_ENTIRE_DEVICE_PATH (p))
      break;
  }
  return total_size;
}

grub_efi_device_path_protocol_t*
grub_efi_create_device_node (grub_efi_uint8_t node_type, grub_efi_uintn_t node_subtype,
                    grub_efi_uint16_t node_length)
{
  grub_efi_device_path_protocol_t *dp;
  if (node_length < sizeof (grub_efi_device_path_protocol_t))
    return NULL;
  dp = grub_zalloc (node_length);
  if (dp != NULL)
  {
    dp->type = node_type;
    dp->subtype = node_subtype;
    set_device_path_node_length (dp, node_length);
  }
  return dp;
}

grub_efi_device_path_protocol_t*
grub_efi_append_device_path (const grub_efi_device_path_protocol_t *dp1,
                    const grub_efi_device_path_protocol_t *dp2)
{
  grub_efi_uintn_t size;
  grub_efi_uintn_t size1;
  grub_efi_uintn_t size2;
  grub_efi_device_path_protocol_t *new_dp;
  grub_efi_device_path_protocol_t *tmp_dp;
  // If there's only 1 path, just duplicate it.
  if (dp1 == NULL)
  {
    if (dp2 == NULL)
      return grub_efi_create_device_node (GRUB_EFI_END_DEVICE_PATH_TYPE,
                                 GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE,
                                 sizeof (grub_efi_device_path_protocol_t));
    else
      return grub_efi_duplicate_device_path (dp2);
  }
  if (dp2 == NULL)
    grub_efi_duplicate_device_path (dp1);
  // Allocate space for the combined device path. It only has one end node of
  // length EFI_DEVICE_PATH_PROTOCOL.
  size1 = grub_efi_get_dp_size (dp1);
  size2 = grub_efi_get_dp_size (dp2);
  size = size1 + size2 - sizeof (grub_efi_device_path_protocol_t);
  new_dp = grub_malloc (size);

  if (new_dp != NULL)
  {
    new_dp = grub_memcpy (new_dp, dp1, size1);
    // Over write FirstDevicePath EndNode and do the copy
    tmp_dp = (grub_efi_device_path_protocol_t *)
           ((char *) new_dp + (size1 - sizeof (grub_efi_device_path_protocol_t)));
    grub_memcpy (tmp_dp, dp2, size2);
  }
  return new_dp;
}

grub_efi_device_path_protocol_t*
grub_efi_append_device_node (const grub_efi_device_path_protocol_t *device_path,
                    const grub_efi_device_path_protocol_t *device_node)
{
  grub_efi_device_path_protocol_t *tmp_dp;
  grub_efi_device_path_protocol_t *next_node;
  grub_efi_device_path_protocol_t *new_dp;
  grub_efi_uintn_t node_length;
  if (device_node == NULL)
  {
    if (device_path == NULL)
      return grub_efi_create_device_node (GRUB_EFI_END_DEVICE_PATH_TYPE,
                                 GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE,
                                 sizeof (grub_efi_device_path_protocol_t));
    else
      return grub_efi_duplicate_device_path (device_path);
  }
  // Build a Node that has a terminator on it
  node_length = device_path_node_length (device_node);

  tmp_dp = grub_malloc (node_length + sizeof (grub_efi_device_path_protocol_t));
  if (tmp_dp == NULL)
    return NULL;
  tmp_dp = grub_memcpy (tmp_dp, device_node, node_length);
  // Add and end device path node to convert Node to device path
  next_node = GRUB_EFI_NEXT_DEVICE_PATH (tmp_dp);
  next_node->type = GRUB_EFI_END_DEVICE_PATH_TYPE;
  next_node->subtype = GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE;
  next_node->length = sizeof (grub_efi_device_path_protocol_t);
  // Append device paths
  new_dp = grub_efi_append_device_path (device_path, tmp_dp);
  grub_free (tmp_dp);
  return new_dp;
}

static grub_err_t
grub_cmd_dp (grub_extcmd_context_t ctxt __attribute__ ((unused)),
             int argc, char **args)
{
  if (argc != 1)
    return 0;
  char *text_dp = NULL;
  char *filename = NULL;
  char *devname = NULL;
  grub_device_t dev = 0;
  grub_efi_device_path_t *dp = NULL;
  grub_efi_device_path_t *file_dp = NULL;
  grub_efi_handle_t dev_handle = 0;
  int namelen = grub_strlen (args[0]);
  if (args[0][0] == '(' && args[0][namelen - 1] == ')')
  {
    args[0][namelen - 1] = 0;
    devname = &args[0][1];
  }
  else if (args[0][0] != '(' && args[0][0] != '/')
    devname = args[0];
  else
  {
    filename = args[0];
    devname = grub_file_get_device_name (filename);
  }
  dev = grub_device_open (devname);
  if (dev->disk)
    dev_handle = grub_efidisk_get_device_handle (dev->disk);
  else if (dev->net && dev->net->server)
  {
    grub_net_network_level_address_t addr;
    struct grub_net_network_level_interface *inf;
    grub_net_network_level_address_t gateway;
    grub_err_t err;
    err = grub_net_resolve_address (dev->net->server, &addr);
    if (err)
      goto out;
    err = grub_net_route_address (addr, &gateway, &inf);
    if (err)
      goto out;
    dev_handle = grub_efinet_get_device_handle (inf->card);
  }
  if (dev_handle)
    dp = grub_efi_get_device_path (dev_handle);
  if (filename)
    file_dp = grub_efi_file_device_path (dp, filename);
  else
    file_dp = grub_efi_duplicate_device_path (dp);
out:
  grub_printf ("DevicePath: ");
  if (!file_dp)
    grub_printf ("NULL\n");
  else
  {
    text_dp = grub_efi_device_path_to_str (file_dp);
    grub_printf ("%s\n", text_dp);
    if (text_dp)
      grub_free (text_dp);
    grub_free (file_dp);
  }

  if (dev)
    grub_device_close (dev);
  return 0;
}

static grub_extcmd_t cmd_dp;

GRUB_MOD_INIT(dp)
{
  cmd_dp = grub_register_extcmd ("dp", grub_cmd_dp, 0, N_("DEVICE"),
                  N_("DevicePath."), 0);
}

GRUB_MOD_FINI(dp)
{
  grub_unregister_extcmd (cmd_dp);
}
